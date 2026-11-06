#ifndef _FM_PSARC_H_
#define _FM_PSARC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*PsarcListCallback)(const char *name, int isDir, uint64_t size, void *userdata);

int psarcOpen(const char *path);
int psarcClose(void);
int psarcIsOpen(void);
int psarcContainsPath(const char *path);
int psarcList(const char *path, PsarcListCallback callback, void *userdata);

#ifdef __cplusplus
}
#endif

#endif
