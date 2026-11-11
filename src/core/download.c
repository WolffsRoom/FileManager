#include <psp2/net/http.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>

#include "download.h"

#define FM_USER_AGENT "FileManager/0.8 libhttp/1.1"

int downloadUrl(const char *url, const char *destination, DownloadProgress progress)
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
	if (result < 0) sceIoRemove(destination);
	return result;
}
