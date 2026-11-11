#ifndef _FM_MEDIA_EXPORT_H_
#define _FM_MEDIA_EXPORT_H_

#include "core/fileops.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Exports MP3/audio or MP4 video to the matching native Vita library. */
int exportMediaFile(const char *path, int video, FileProcessParam *param);

#ifdef __cplusplus
}
#endif

#endif
