#ifndef _FM_INSTALL_H_
#define _FM_INSTALL_H_

#include "core/fileops.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FM_PACKAGE_DIR "ux0:data/pkg"

/**
 * Installs a VPK as FMVita/VitaShell do: extract to FM_PACKAGE_DIR, write a fake
 * head.bin, promote with ScePromoterUtil. Progress counts extracted bytes.
 * Returns 1 on success, 0 when cancelled, a negative error otherwise.
 * `titleid` (12 bytes) receives the installed title id.
 */
int installVpk(const char *vpk, FileProcessParam *param, char *titleid);

typedef struct {
  char title[128];
  char version[16];
  char titleid[12];
  void *icon;          /* icon0.png, malloc'ed, or NULL */
  uint32_t iconSize;
} VpkInfo;

/** Reads sce_sys/param.sfo and sce_sys/icon0.png out of a VPK without extracting it. */
int readVpkInfo(const char *vpk, VpkInfo *info);

/** Total uncompressed size of an archive, for the progress bar. Negative on error. */
int64_t archiveTotalSize(const char *path);

#ifdef __cplusplus
}
#endif

#endif
