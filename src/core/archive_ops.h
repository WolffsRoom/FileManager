#ifndef _FM_ARCHIVE_OPS_H_
#define _FM_ARCHIVE_OPS_H_

#include "core/fileops.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Extracts ZIP/RAR/7z/tar and every other libarchive readable format. */
int archiveExtract(const char *archivePath, const char *destination, FileProcessParam *param);

/** Creates a standard Deflate ZIP. RAR creation is intentionally unsupported. */
int archiveCreateZip(const char *zipPath, const char **sources, int sourceCount, FileProcessParam *param);

#ifdef __cplusplus
}
#endif

#endif
