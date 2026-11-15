#include <archive.h>
#include <archive_entry.h>
#include <png.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include "core/vpk_assets.h"
#include "core/fileops.h"

#define PNG_LIMIT (16 * 1024 * 1024)

typedef struct {
	const uint8_t *data;
	size_t size;
	size_t offset;
} PngReader;

typedef struct {
	uint8_t *pixels;
	uint32_t width;
	uint32_t height;
	int palette8;
} DecodedPng;

static void readPng(png_structp png, png_bytep out, png_size_t count)
{
	PngReader *reader = (PngReader *)png_get_io_ptr(png);
	if (!reader || reader->offset + count > reader->size) png_error(png, "truncated PNG");
	memcpy(out, reader->data + reader->offset, count);
	reader->offset += count;
}

static void writePng(png_structp png, png_bytep data, png_size_t count)
{
	SceUID *fd = (SceUID *)png_get_io_ptr(png);
	if (!fd || sceIoWrite(*fd, data, count) != (int)count) png_error(png, "PNG write failed");
}

static void flushPng(png_structp png) { (void)png; }

static int decodePng(const void *data, size_t size, DecodedPng *decoded)
{
	memset(decoded, 0, sizeof(*decoded));
	if (!data || size < 33 || png_sig_cmp((png_const_bytep)data, 0, 8)) return -1;
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) return -1;
	png_infop info = png_create_info_struct(png);
	png_bytep *rows = NULL;
	PngReader reader = { (const uint8_t *)data, size, 0 };
	if (!info || setjmp(png_jmpbuf(png)))
	{
		free(rows);
		free(decoded->pixels);
		memset(decoded, 0, sizeof(*decoded));
		png_destroy_read_struct(&png, info ? &info : NULL, NULL);
		return -1;
	}
	png_set_read_fn(png, &reader, readPng);
	png_read_info(png, info);
	decoded->width = png_get_image_width(png, info);
	decoded->height = png_get_image_height(png, info);
	int depth = png_get_bit_depth(png, info);
	int color = png_get_color_type(png, info);
	decoded->palette8 = depth == 8 && color == PNG_COLOR_TYPE_PALETTE;
	if (!decoded->width || !decoded->height || decoded->width > 4096 || decoded->height > 4096)
		png_error(png, "invalid dimensions");
	if (depth == 16) png_set_strip_16(png);
	if (color == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
	if (color == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
	if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
	if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
	if (!(color & PNG_COLOR_MASK_ALPHA) && !png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);
	png_read_update_info(png, info);
	if (png_get_rowbytes(png, info) != decoded->width * 4) png_error(png, "unexpected row format");
	decoded->pixels = (uint8_t *)malloc((size_t)decoded->width * decoded->height * 4);
	rows = (png_bytep *)malloc(sizeof(png_bytep) * decoded->height);
	if (!decoded->pixels || !rows) png_error(png, "out of memory");
	for (uint32_t y = 0; y < decoded->height; ++y)
		rows[y] = decoded->pixels + (size_t)y * decoded->width * 4;
	png_read_image(png, rows);
	png_read_end(png, info);
	free(rows);
	png_destroy_read_struct(&png, &info, NULL);
	return 0;
}

static int isAssetPath(const char *path)
{
	if (!path) return 0;
	size_t length = strlen(path);
	if (length < 5 || strcasecmp(path + length - 4, ".png") != 0) return 0;
	if (strcmp(path, "sce_sys/icon0.png") == 0 || strcmp(path, "sce_sys/pic0.png") == 0) return 1;
	return strncmp(path, "sce_sys/livearea/contents/", 26) == 0;
}

static int dimensionsValid(const char *path, uint32_t width, uint32_t height)
{
	if (strcmp(path, "sce_sys/icon0.png") == 0) return width == 128 && height == 128;
	const char *name = strrchr(path, '/');
	name = name ? name + 1 : path;
	if (strcasecmp(name, "startup.png") == 0) return width == 280 && height == 158;
	return 1;
}

static void reportInvalid(VpkAssetReport *report, const char *path)
{
	++report->invalid;
	if (!report->firstInvalid[0])
	{
		strncpy(report->firstInvalid, path, sizeof(report->firstInvalid) - 1);
		report->firstInvalid[sizeof(report->firstInvalid) - 1] = 0;
	}
}

static void inspectBuffer(const char *path, const void *data, size_t size, VpkAssetReport *report)
{
	DecodedPng decoded;
	if (decodePng(data, size, &decoded) < 0 || !dimensionsValid(path, decoded.width, decoded.height))
		reportInvalid(report, path);
	else if (decoded.palette8)
		++report->compatible;
	else
		++report->fixable;
	free(decoded.pixels);
}

int inspectVpkAssets(const char *vpk, VpkAssetReport *report)
{
	if (!vpk || !report) return -1;
	memset(report, 0, sizeof(*report));
	struct archive *reader = archive_read_new();
	archive_read_support_format_zip(reader);
	if (archive_read_open_filename(reader, vpk, 64 * 1024) != ARCHIVE_OK)
	{
		archive_read_free(reader);
		return -1;
	}
	struct archive_entry *entry;
	while (archive_read_next_header(reader, &entry) == ARCHIVE_OK)
	{
		const char *path = archive_entry_pathname(entry);
		if (!isAssetPath(path)) { archive_read_data_skip(reader); continue; }
		int64_t size = archive_entry_size(entry);
		if (size <= 0 || size > PNG_LIMIT) { reportInvalid(report, path); continue; }
		void *data = malloc((size_t)size);
		if (!data || archive_read_data(reader, data, (size_t)size) != size)
		{
			free(data);
			reportInvalid(report, path);
			continue;
		}
		inspectBuffer(path, data, (size_t)size, report);
		free(data);
	}
	archive_read_free(reader);
	return report->invalid ? -1 : 0;
}

static int readFile(const char *path, void **out, size_t *size)
{
	SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0) return fd;
	SceOff end = sceIoLseek(fd, 0, SCE_SEEK_END);
	if (end <= 0 || end > PNG_LIMIT) { sceIoClose(fd); return -1; }
	sceIoLseek(fd, 0, SCE_SEEK_SET);
	void *data = malloc((size_t)end);
	if (!data) { sceIoClose(fd); return -1; }
	int got = sceIoRead(fd, data, (size_t)end);
	sceIoClose(fd);
	if (got != end) { free(data); return got < 0 ? got : -1; }
	*out = data;
	*size = (size_t)end;
	return 0;
}

static uint8_t paletteIndex(const uint8_t *rgba)
{
	if (rgba[3] < 128) return 0;
	uint8_t index = (uint8_t)((rgba[0] & 0xe0) | ((rgba[1] & 0xe0) >> 3) | (rgba[2] >> 6));
	return index ? index : 1;
}

static void makePalette(png_color palette[256])
{
	memset(palette, 0, sizeof(png_color) * 256);
	for (int i = 1; i < 256; ++i)
	{
		palette[i].red = (png_byte)(((i >> 5) * 255) / 7);
		palette[i].green = (png_byte)((((i >> 2) & 7) * 255) / 7);
		palette[i].blue = (png_byte)(((i & 3) * 255) / 3);
	}
	palette[1].red = palette[1].green = palette[1].blue = 0;
}

static int writeIndexed(const char *path, const DecodedPng *decoded)
{
	char temporary[MAX_PATH_LENGTH];
	char backup[MAX_PATH_LENGTH];
	snprintf(temporary, sizeof(temporary), "%s.fm9tmp", path);
	snprintf(backup, sizeof(backup), "%s.fm9bak", path);
	sceIoRemove(temporary);
	sceIoRemove(backup);
	SceUID fd = sceIoOpen(temporary, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
	if (fd < 0) return fd;
	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info = png ? png_create_info_struct(png) : NULL;
	uint8_t *indices = NULL;
	png_bytep *rows = NULL;
	int result = -1;
	if (!png || !info || setjmp(png_jmpbuf(png))) goto done;
	png_set_write_fn(png, &fd, writePng, flushPng);
	png_set_IHDR(png, info, decoded->width, decoded->height, 8, PNG_COLOR_TYPE_PALETTE,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_color palette[256];
	makePalette(palette);
	png_set_PLTE(png, info, palette, 256);
	png_byte transparency[1] = { 0 };
	png_set_tRNS(png, info, transparency, 1, NULL);
	indices = (uint8_t *)malloc((size_t)decoded->width * decoded->height);
	rows = (png_bytep *)malloc(sizeof(png_bytep) * decoded->height);
	if (!indices || !rows) png_error(png, "out of memory");
	for (uint32_t y = 0; y < decoded->height; ++y)
	{
		rows[y] = indices + (size_t)y * decoded->width;
		for (uint32_t x = 0; x < decoded->width; ++x)
			rows[y][x] = paletteIndex(decoded->pixels + ((size_t)y * decoded->width + x) * 4);
	}
	png_write_info(png, info);
	png_write_image(png, rows);
	png_write_end(png, info);
	result = 0;
done:
	free(rows);
	free(indices);
	if (png) png_destroy_write_struct(&png, info ? &info : NULL);
	sceIoClose(fd);
	if (result < 0) { sceIoRemove(temporary); return result; }
	result = sceIoRename(path, backup);
	if (result < 0) { sceIoRemove(temporary); return result; }
	result = sceIoRename(temporary, path);
	if (result < 0)
	{
		sceIoRemove(path);
		sceIoRename(backup, path);
		sceIoRemove(temporary);
		return result;
	}
	sceIoRemove(backup);
	return result;
}

static int verifyIndexedAsset(const char *path, const char *relative)
{
	void *data = NULL;
	size_t size = 0;
	if (readFile(path, &data, &size) < 0) return -1;
	DecodedPng decoded;
	int result = decodePng(data, size, &decoded);
	free(data);
	if (result < 0) return -1;
	result = decoded.palette8 && dimensionsValid(relative, decoded.width, decoded.height) ? 0 : -1;
	free(decoded.pixels);
	return result;
}

static int normalizeOne(const char *root, const char *relative, VpkAssetReport *report)
{
	char path[MAX_PATH_LENGTH];
	snprintf(path, sizeof(path), "%s/%s", root, relative);
	void *data = NULL;
	size_t size = 0;
	if (readFile(path, &data, &size) < 0) { reportInvalid(report, relative); return -1; }
	DecodedPng decoded;
	if (decodePng(data, size, &decoded) < 0 || !dimensionsValid(relative, decoded.width, decoded.height))
	{
		free(data);
		free(decoded.pixels);
		reportInvalid(report, relative);
		return -1;
	}
	free(data);
	if (decoded.palette8) ++report->compatible;
	else if (writeIndexed(path, &decoded) < 0 || verifyIndexedAsset(path, relative) < 0)
		reportInvalid(report, relative);
	else ++report->fixable;
	free(decoded.pixels);
	return report->invalid ? -1 : 0;
}

static int normalizeDirectory(const char *root, const char *relativeDir, VpkAssetReport *report)
{
	char directory[MAX_PATH_LENGTH];
	snprintf(directory, sizeof(directory), "%s/%s", root, relativeDir);
	SceUID dfd = sceIoDopen(directory);
	if (dfd < 0) return 0;
	SceIoDirent entry;
	while (1)
	{
		memset(&entry, 0, sizeof(entry));
		int read = sceIoDread(dfd, &entry);
		if (read <= 0) break;
		size_t length = strlen(entry.d_name);
		if (!SCE_S_ISDIR(entry.d_stat.st_mode) && length > 4 && strcasecmp(entry.d_name + length - 4, ".png") == 0)
		{
			char relative[MAX_PATH_LENGTH];
			snprintf(relative, sizeof(relative), "%s/%s", relativeDir, entry.d_name);
			if (normalizeOne(root, relative, report) < 0) break;
		}
	}
	sceIoDclose(dfd);
	return report->invalid ? -1 : 0;
}

int normalizeExtractedVpkAssets(const char *root, VpkAssetReport *report)
{
	if (!root || !report) return -1;
	memset(report, 0, sizeof(*report));
	char icon[MAX_PATH_LENGTH];
	SceIoStat stat;
	snprintf(icon, sizeof(icon), "%s/sce_sys/icon0.png", root);
	if (sceIoGetstat(icon, &stat) >= 0 && normalizeOne(root, "sce_sys/icon0.png", report) < 0) return -1;
	snprintf(icon, sizeof(icon), "%s/sce_sys/pic0.png", root);
	if (sceIoGetstat(icon, &stat) >= 0 && normalizeOne(root, "sce_sys/pic0.png", report) < 0) return -1;
	return normalizeDirectory(root, "sce_sys/livearea/contents", report);
}
