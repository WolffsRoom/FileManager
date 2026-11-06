#ifndef _FM_PFS_H_
#define _FM_PFS_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int pfsInitModules(void);
int pfsMount(const char *path);
int pfsUmount(void);
const char *pfsMountPoint(void);
int pfsIsProtectedContainer(const char *path);

#ifdef __cplusplus
}
#endif

#endif
