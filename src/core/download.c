#include <psp2/net/http.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "download.h"

#define FM_USER_AGENT "FileManager/0.9 libhttp/1.1"

static void copyHeaderValue(const char *headers, unsigned int headersSize,
	const char *field, char *out, size_t outSize)
{
	const char *value = NULL;
	unsigned int valueSize = 0;
	out[0] = 0;
	if (sceHttpParseResponseHeader(headers, headersSize, field, &value, &valueSize) < 0 || !value)
		return;
	if (valueSize >= outSize) valueSize = (unsigned int)outSize - 1;
	memcpy(out, value, valueSize);
	out[valueSize] = 0;
}

static int hexValue(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static const char *findCase(const char *text, const char *needle)
{
	size_t length = strlen(needle);
	if (!length) return text;
	for (; *text; ++text)
		if (strncasecmp(text, needle, length) == 0) return text;
	return NULL;
}

static void decodeFilename(char *out, size_t outSize, const char *src, size_t srcSize)
{
	size_t n = 0;
	while (srcSize && (*src == ' ' || *src == '\t' || *src == '"')) { ++src; --srcSize; }
	while (srcSize && n + 1 < outSize)
	{
		char c = *src++;
		--srcSize;
		if (c == '"' || c == ';' || c == '\r' || c == '\n') break;
		if (c == '%' && srcSize >= 2)
		{
			int hi = hexValue(src[0]), lo = hexValue(src[1]);
			if (hi >= 0 && lo >= 0) { c = (char)((hi << 4) | lo); src += 2; srcSize -= 2; }
		}
		if ((unsigned char)c < 0x20 || c == '\\' || c == '/' || c == ':' || c == '*' ||
			c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
		out[n++] = c;
	}
	while (n && (out[n - 1] == ' ' || out[n - 1] == '.')) --n;
	out[n] = 0;
}

static void filenameFromDisposition(const char *value, char *out, size_t outSize)
{
	out[0] = 0;
	const char *key = findCase(value, "filename*=");
	if (key)
	{
		key += 10;
		const char *encoded = strstr(key, "''");
		if (encoded) key = encoded + 2;
		decodeFilename(out, outSize, key, strlen(key));
		if (out[0]) return;
	}
	key = findCase(value, "filename=");
	if (key) decodeFilename(out, outSize, key + 9, strlen(key + 9));
}

static void filenameFromUrl(const char *url, char *out, size_t outSize)
{
	const char *slash = strrchr(url, '/');
	const char *name = slash && slash[1] ? slash + 1 : "";
	size_t len = strcspn(name, "?#");
	decodeFilename(out, outSize, name, len);
}

static int urlLooksLikeFile(const char *name)
{
	const char *dot = strrchr(name, '.');
	if (!dot || !dot[1]) return 0;
	return strcasecmp(dot, ".html") != 0 && strcasecmp(dot, ".htm") != 0 &&
		strcasecmp(dot, ".php") != 0 && strcasecmp(dot, ".asp") != 0 &&
		strcasecmp(dot, ".aspx") != 0;
}

int inspectDownloadUrl(const char *url, DownloadInfo *info)
{
	int tmpl = -1, conn = -1, req = -1, result = -1, status = 0;
	char disposition[384] = { 0 };
	char *headers = NULL;
	unsigned int headersSize = 0;
	if (!url || !info) return -1;
	memset(info, 0, sizeof(*info));

	tmpl = sceHttpCreateTemplate(FM_USER_AGENT, SCE_HTTP_VERSION_1_1, SCE_TRUE);
	if (tmpl < 0) { result = tmpl; goto done; }
	sceHttpSetAutoRedirect(tmpl, SCE_TRUE);
	conn = sceHttpCreateConnectionWithURL(tmpl, url, SCE_TRUE);
	if (conn < 0) { result = conn; goto done; }
	req = sceHttpCreateRequestWithURL(conn, SCE_HTTP_METHOD_GET, url, 0);
	if (req < 0) { result = req; goto done; }
	result = sceHttpSendRequest(req, NULL, 0);
	if (result < 0) goto done;
	result = sceHttpGetStatusCode(req, &status);
	if (result < 0) goto done;
	if (status < 200 || status >= 300) { result = -status; goto done; }
	sceHttpGetResponseContentLength(req, &info->contentLength);
	result = sceHttpGetAllResponseHeaders(req, &headers, &headersSize);
	if (result >= 0 && headers)
	{
		copyHeaderValue(headers, headersSize, "Content-Disposition", disposition, sizeof(disposition));
		copyHeaderValue(headers, headersSize, "Content-Type", info->contentType, sizeof(info->contentType));
	}
	filenameFromDisposition(disposition, info->filename, sizeof(info->filename));
	if (!info->filename[0]) filenameFromUrl(url, info->filename, sizeof(info->filename));
	if (!info->filename[0]) strcpy(info->filename, "download.bin");

	const int attachment = findCase(disposition, "attachment") != NULL ||
		findCase(disposition, "filename=") != NULL || findCase(disposition, "filename*=") != NULL;
	const int html = strncasecmp(info->contentType, "text/html", 9) == 0 ||
		strncasecmp(info->contentType, "application/xhtml", 17) == 0;
	const int typedFile = info->contentType[0] && !html;
	info->isFile = attachment || typedFile || urlLooksLikeFile(info->filename);
	result = 0;

done:
	if (req >= 0) sceHttpDeleteRequest(req);
	if (conn >= 0) sceHttpDeleteConnection(conn);
	if (tmpl >= 0) sceHttpDeleteTemplate(tmpl);
	return result;
}

int downloadUrlEx(const char *url, const char *destination, DownloadProgress progress, DownloadCancel cancel)
{
	int tmpl = -1, conn = -1, req = -1, fd = -1, result = -1;
	int status = 0;
	uint64_t total = 0, value = 0;

	tmpl = sceHttpCreateTemplate(FM_USER_AGENT, SCE_HTTP_VERSION_1_1, SCE_TRUE);
	if (tmpl < 0) { result = tmpl; goto done; }
	sceHttpSetAutoRedirect(tmpl, SCE_TRUE);
	conn = sceHttpCreateConnectionWithURL(tmpl, url, SCE_TRUE);
	if (conn < 0) { result = conn; goto done; }
	req = sceHttpCreateRequestWithURL(conn, SCE_HTTP_METHOD_GET, url, 0);
	if (req < 0) { result = req; goto done; }
	result = sceHttpSendRequest(req, NULL, 0);
	if (result < 0) goto done;
	result = sceHttpGetStatusCode(req, &status);
	if (result < 0) goto done;
	if (status < 200 || status >= 300) { result = -status; goto done; }
	sceHttpGetResponseContentLength(req, &total);

	fd = sceIoOpen(destination, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
	if (fd < 0) { result = fd; goto done; }
	for (;;)
	{
		if (cancel && cancel()) { result = 1; goto done; }
		unsigned char buffer[32 * 1024];
		int got = sceHttpReadData(req, buffer, sizeof(buffer));
		if (got < 0) { result = got; goto done; }
		if (!got) break;
		int written = sceIoWrite(fd, buffer, got);
		if (written != got) { result = written < 0 ? written : -1; goto done; }
		value += got;
		if (progress) progress(value, total);
	}
	result = 0;

done:
	if (fd >= 0) sceIoClose(fd);
	if (req >= 0) sceHttpDeleteRequest(req);
	if (conn >= 0) sceHttpDeleteConnection(conn);
	if (tmpl >= 0) sceHttpDeleteTemplate(tmpl);
	if (result != 0) sceIoRemove(destination);
	return result;
}

int downloadUrl(const char *url, const char *destination, DownloadProgress progress)
{
	return downloadUrlEx(url, destination, progress, NULL);
}
