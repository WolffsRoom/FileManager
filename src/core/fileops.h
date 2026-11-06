#ifndef _FM_FILEOPS_H_
#define _FM_FILEOPS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SCE_ERROR_ERRNO_EEXIST
#define SCE_ERROR_ERRNO_EEXIST 0x80010011
#endif

#define MAX_PATH_LENGTH 1024
#define DIRECTORY_SIZE (4 * 1024)
#define TRANSFER_SIZE (256 * 1024)

enum FileMoveFlags {
  MOVE_INTEGRATE = 0x1, // Integrate directories
  MOVE_REPLACE   = 0x2, // Replace files
};

typedef struct {
  uint64_t *value;
  uint64_t max;
  void (*SetProgress)(uint64_t value, uint64_t max);
  int (*cancelHandler)();
} FileProcessParam;

// Each returns 1 on success, 0 when cancelled, a negative error otherwise.
int getPathInfo(const char *path, uint64_t *size, uint32_t *folders, uint32_t *files, int (*handler)(const char *path));
int removePath(const char *path, FileProcessParam *param);
int copyFile(const char *src_path, const char *dst_path, FileProcessParam *param);
int copyPath(const char *src_path, const char *dst_path, FileProcessParam *param);
int movePath(const char *src_path, const char *dst_path, int flags, FileProcessParam *param);

#ifdef __cplusplus
}
#endif

#endif
