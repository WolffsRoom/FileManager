#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <zlib.h>

#include "core/bubble.h"

typedef struct {
  const uint8_t *data;
  uint32_t size, pos;
} Reader;

static void readCb(png_structp png, png_bytep out, png_size_t len) {
  Reader *r = (Reader *)png_get_io_ptr(png);
  if (r->pos + len > r->size)
    png_error(png, "eof");
  memcpy(out, r->data + r->pos, len);
  r->pos += len;
}

static uint8_t *decode(const void *data, uint32_t size, int *w, int *h) {
  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png)
    return NULL;
  png_infop info = png_create_info_struct(png);
  uint8_t *pixels = NULL;
  png_bytep *rows = NULL;
  Reader r = { (const uint8_t *)data, size, 0 };
  if (setjmp(png_jmpbuf(png))) {
    free(pixels);
    free(rows);
    png_destroy_read_struct(&png, &info, NULL);
    return NULL;
  }
  png_set_read_fn(png, &r, readCb);
  png_read_info(png, info);
  png_set_expand(png);
  png_set_strip_16(png);
  png_set_gray_to_rgb(png);
  png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
  png_read_update_info(png, info);
  *w = png_get_image_width(png, info);
  *h = png_get_image_height(png, info);
  pixels = malloc((*w) * (*h) * 4);
  rows = malloc(sizeof(png_bytep) * (*h));
  for (int y = 0; y < *h; y++)
    rows[y] = pixels + y * (*w) * 4;
  png_read_image(png, rows);
  free(rows);
  png_destroy_read_struct(&png, &info, NULL);
  return pixels;
}

static void put32(uint8_t *p, uint32_t v) {
  p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static uint32_t chunk(uint8_t *dst, const char *tag, const uint8_t *data, uint32_t len) {
  put32(dst, len);
  memcpy(dst + 4, tag, 4);
  if (len)
    memcpy(dst + 8, data, len);
  put32(dst + 8 + len, crc32(0, dst + 4, len + 4));
  return len + 12;
}

int makeBubblePng(const void *png, uint32_t size, void **out, uint32_t *out_size) {
  int w = 0, h = 0;
  uint8_t *px = decode(png, size, &w, &h);
  if (!px)
    return -1;

  // Circle with a 1.5 px soft edge, and a gloss over the upper half.
  const float cx = (w - 1) / 2.0f, cy = (h - 1) / 2.0f, rad = (w < h ? w : h) / 2.0f - 1.0f;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      uint8_t *p = px + (y * w + x) * 4;
      float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy));
      float a = rad - d + 0.75f;
      a = a < 0 ? 0 : (a > 1.5f ? 1.0f : a / 1.5f);
      p[3] = (uint8_t)(p[3] * a);
      float gy = (cy - y) / rad, gx = (x - cx) / rad;
      if (gy > 0.15f && gx * gx + (gy - 0.55f) * (gy - 0.55f) * 3.5f < 0.55f) {
        for (int k = 0; k < 3; k++)
          p[k] = (uint8_t)(p[k] + (255 - p[k]) * 0.22f);
      }
    }
  }

  // Encode as a plain RGBA PNG.
  uint32_t rawlen = (uint32_t)h * (w * 4 + 1);
  uint8_t *raw = malloc(rawlen);
  for (int y = 0; y < h; y++) {
    raw[y * (w * 4 + 1)] = 0;
    memcpy(raw + y * (w * 4 + 1) + 1, px + y * w * 4, w * 4);
  }
  free(px);
  uLongf zlen = compressBound(rawlen);
  uint8_t *z = malloc(zlen);
  compress2(z, &zlen, raw, rawlen, 3);
  free(raw);

  uint8_t *buf = malloc(8 + 25 + 12 + zlen + 12);
  static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
  memcpy(buf, sig, 8);
  uint8_t ihdr[13];
  put32(ihdr, w);
  put32(ihdr + 4, h);
  ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
  uint32_t n = 8;
  n += chunk(buf + n, "IHDR", ihdr, 13);
  n += chunk(buf + n, "IDAT", z, (uint32_t)zlen);
  n += chunk(buf + n, "IEND", NULL, 0);
  free(z);

  *out = buf;
  *out_size = n;
  return 0;
}
