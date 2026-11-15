#ifndef _FM_DOWNLOAD_H_
#define _FM_DOWNLOAD_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*DownloadProgress)(uint64_t value, uint64_t total);
typedef int (*DownloadCancel)(void);

typedef struct DownloadInfo {
	char filename[256];
	char contentType[96];
	uint64_t contentLength;
	int isFile;
} DownloadInfo;

/** Resolves redirects and classifies the response using URL and HTTP metadata. */
int inspectDownloadUrl(const char *url, DownloadInfo *info);

int downloadUrl(const char *url, const char *destination, DownloadProgress progress);
int downloadUrlEx(const char *url, const char *destination, DownloadProgress progress, DownloadCancel cancel);

#ifdef __cplusplus
}
#endif

#endif
