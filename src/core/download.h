#ifndef _FM_DOWNLOAD_H_
#define _FM_DOWNLOAD_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*DownloadProgress)(uint64_t value, uint64_t total);

int downloadUrl(const char *url, const char *destination, DownloadProgress progress);

#ifdef __cplusplus
}
#endif

#endif
