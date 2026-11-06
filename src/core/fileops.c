/*
  VitaShell
  Copyright (C) 2015-2018, TheFloW

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// File operations extracted from FMVita/VitaShell file.c, free of UI code.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <malloc.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>

#include "legacy/vitashell_error.h"
#include "core/fileops.h"

static int hasEndSlash(const char *path) {
  return path[strlen(path) - 1] == '/';
}

int getPathInfo(const char *path, uint64_t *size, uint32_t *folders,
                uint32_t *files, int (* handler)(const char *path)) {
  SceUID dfd = sceIoDopen(path);
  if (dfd >= 0) {
    int res = 0;

    do {
      SceIoDirent dir;
      memset(&dir, 0, sizeof(SceIoDirent));

      res = sceIoDread(dfd, &dir);
      if (res > 0) {
        char *new_path = malloc(strlen(path) + strlen(dir.d_name) + 2);
        snprintf(new_path, MAX_PATH_LENGTH, "%s%s%s", path, hasEndSlash(path) ? "" : "/", dir.d_name);

        if (handler && handler(new_path)) {
          free(new_path);
          continue;
        }

        if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
          int ret = getPathInfo(new_path, size, folders, files, handler);
          if (ret <= 0) {
            free(new_path);
            sceIoDclose(dfd);
            return ret;
          }
        } else {
          if (size)
            (*size) += dir.d_stat.st_size;

          if (files)
            (*files)++;
        }

        free(new_path);
      }
    } while (res > 0);

    sceIoDclose(dfd);

    if (folders)
      (*folders)++;
  } else {
    if (handler && handler(path))
      return 1;

    if (size) {
      SceIoStat stat;
      memset(&stat, 0, sizeof(SceIoStat));

      int res = sceIoGetstat(path, &stat);
      if (res < 0)
        return res;

      (*size) += stat.st_size;
    }

    if (files)
      (*files)++;
  }

  return 1;
}

int removePath(const char *path, FileProcessParam *param) {
  SceUID dfd = sceIoDopen(path);
  if (dfd >= 0) {
    int res = 0;

    do {
      SceIoDirent dir;
      memset(&dir, 0, sizeof(SceIoDirent));

      res = sceIoDread(dfd, &dir);
      if (res > 0) {
        char *new_path = malloc(strlen(path) + strlen(dir.d_name) + 2);
        snprintf(new_path, MAX_PATH_LENGTH, "%s%s%s", path, hasEndSlash(path) ? "" : "/", dir.d_name);

        if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
          int ret = removePath(new_path, param);
          if (ret <= 0) {
            free(new_path);
            sceIoDclose(dfd);
            return ret;
          }
        } else {
          int ret = sceIoRemove(new_path);
          if (ret < 0) {
            free(new_path);
            sceIoDclose(dfd);
            return ret;
          }

          if (param) {
            if (param->value)
              (*param->value)++;

            if (param->SetProgress)
              param->SetProgress(param->value ? *param->value : 0, param->max);

            if (param->cancelHandler && param->cancelHandler()) {
              free(new_path);
              sceIoDclose(dfd);
              return 0;
            }
          }
        }

        free(new_path);
      }
    } while (res > 0);

    sceIoDclose(dfd);

    int ret = sceIoRmdir(path);
    if (ret < 0)
      return ret;

    if (param) {
      if (param->value)
        (*param->value)++;

      if (param->SetProgress)
        param->SetProgress(param->value ? *param->value : 0, param->max);

      if (param->cancelHandler && param->cancelHandler()) {
        return 0;
      }
    }
  } else {
    int ret = sceIoRemove(path);
    if (ret < 0)
      return ret;

    if (param) {
      if (param->value)
        (*param->value)++;

      if (param->SetProgress)
        param->SetProgress(param->value ? *param->value : 0, param->max);

      if (param->cancelHandler && param->cancelHandler()) {
        return 0;
      }
    }
  }

  return 1;
}

int copyFile(const char *src_path, const char *dst_path, FileProcessParam *param) {
  // The source and destination paths are identical
  if (strcasecmp(src_path, dst_path) == 0) {
    return VITASHELL_ERROR_SRC_AND_DST_IDENTICAL;
  }

  // The destination is a subfolder of the source folder
  int len = strlen(src_path);
  if (strncasecmp(src_path, dst_path, len) == 0 && (dst_path[len] == '/' || dst_path[len - 1] == '/')) {
    return VITASHELL_ERROR_DST_IS_SUBFOLDER_OF_SRC;
  }

  SceUID fdsrc = sceIoOpen(src_path, SCE_O_RDONLY, 0);
  if (fdsrc < 0)
    return fdsrc;

  SceUID fddst = sceIoOpen(dst_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
  if (fddst < 0) {
    sceIoClose(fdsrc);
    return fddst;
  }

  void *buf = memalign(4096, TRANSFER_SIZE);

  while (1) {
    int read = sceIoRead(fdsrc, buf, TRANSFER_SIZE);

    if (read < 0) {
      free(buf);

      sceIoClose(fddst);
      sceIoClose(fdsrc);

      sceIoRemove(dst_path);

      return read;
    }

    if (read == 0)
      break;

    int written = sceIoWrite(fddst, buf, read);

    if (written < 0) {
      free(buf);

      sceIoClose(fddst);
      sceIoClose(fdsrc);

      sceIoRemove(dst_path);

      return written;
    }

    if (param) {
      if (param->value)
        (*param->value) += read;

      if (param->SetProgress)
        param->SetProgress(param->value ? *param->value : 0, param->max);

      if (param->cancelHandler && param->cancelHandler()) {
        free(buf);

        sceIoClose(fddst);
        sceIoClose(fdsrc);

        sceIoRemove(dst_path);

        return 0;
      }
    }
  }

  free(buf);

  // Inherit file stat
  SceIoStat stat;
  memset(&stat, 0, sizeof(SceIoStat));
  sceIoGetstatByFd(fdsrc, &stat);
  sceIoChstatByFd(fddst, &stat, 0x3B);

  sceIoClose(fddst);
  sceIoClose(fdsrc);

  return 1;
}

int copyPath(const char *src_path, const char *dst_path, FileProcessParam *param) {
  // The source and destination paths are identical
  if (strcasecmp(src_path, dst_path) == 0) {
    return VITASHELL_ERROR_SRC_AND_DST_IDENTICAL;
  }

  // The destination is a subfolder of the source folder
  int len = strlen(src_path);
  if (strncasecmp(src_path, dst_path, len) == 0 && (dst_path[len] == '/' || dst_path[len - 1] == '/')) {
    return VITASHELL_ERROR_DST_IS_SUBFOLDER_OF_SRC;
  }

  SceUID dfd = sceIoDopen(src_path);
  if (dfd >= 0) {
    SceIoStat stat;
    memset(&stat, 0, sizeof(SceIoStat));
    sceIoGetstatByFd(dfd, &stat);

    stat.st_mode |= SCE_S_IWUSR;

    int ret = sceIoMkdir(dst_path, stat.st_mode & 0xFFF);
    if (ret < 0 && ret != SCE_ERROR_ERRNO_EEXIST) {
      sceIoDclose(dfd);
      return ret;
    }

    if (ret == SCE_ERROR_ERRNO_EEXIST) {
      sceIoChstat(dst_path, &stat, 0x3B);
    }

    if (param) {
      if (param->value)
        (*param->value) += DIRECTORY_SIZE;

      if (param->SetProgress)
        param->SetProgress(param->value ? *param->value : 0, param->max);

      if (param->cancelHandler && param->cancelHandler()) {
        sceIoDclose(dfd);
        return 0;
      }
    }

    int res = 0;

    do {
      SceIoDirent dir;
      memset(&dir, 0, sizeof(SceIoDirent));

      res = sceIoDread(dfd, &dir);
      if (res > 0) {
        char *new_src_path = malloc(strlen(src_path) + strlen(dir.d_name) + 2);
        snprintf(new_src_path, MAX_PATH_LENGTH, "%s%s%s", src_path, hasEndSlash(src_path) ? "" : "/", dir.d_name);

        char *new_dst_path = malloc(strlen(dst_path) + strlen(dir.d_name) + 2);
        snprintf(new_dst_path, MAX_PATH_LENGTH, "%s%s%s", dst_path, hasEndSlash(dst_path) ? "" : "/", dir.d_name);

        int ret = 0;

        if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
          ret = copyPath(new_src_path, new_dst_path, param);
        } else {
          ret = copyFile(new_src_path, new_dst_path, param);
        }

        free(new_dst_path);
        free(new_src_path);

        if (ret <= 0) {
          sceIoDclose(dfd);
          return ret;
        }
      }
    } while (res > 0);

    sceIoDclose(dfd);
  } else {
    return copyFile(src_path, dst_path, param);
  }

  return 1;
}

int movePath(const char *src_path, const char *dst_path, int flags, FileProcessParam *param) {
  // The source and destination paths are identical
  if (strcasecmp(src_path, dst_path) == 0) {
    return VITASHELL_ERROR_SRC_AND_DST_IDENTICAL;
  }

  // The destination is a subfolder of the source folder
  int len = strlen(src_path);
  if (strncasecmp(src_path, dst_path, len) == 0 && (dst_path[len] == '/' || dst_path[len - 1] == '/')) {
    return VITASHELL_ERROR_DST_IS_SUBFOLDER_OF_SRC;
  }

  int res = sceIoRename(src_path, dst_path);

  if (res == SCE_ERROR_ERRNO_EEXIST && flags & (MOVE_INTEGRATE | MOVE_REPLACE)) {
    // Src stat
    SceIoStat src_stat;
    memset(&src_stat, 0, sizeof(SceIoStat));
    res = sceIoGetstat(src_path, &src_stat);
    if (res < 0)
      return res;

    // Dst stat
    SceIoStat dst_stat;
    memset(&dst_stat, 0, sizeof(SceIoStat));
    res = sceIoGetstat(dst_path, &dst_stat);
    if (res < 0)
      return res;

    // Is dir
    int src_is_dir = SCE_S_ISDIR(src_stat.st_mode);
    int dst_is_dir = SCE_S_ISDIR(dst_stat.st_mode);

    // One of them is a file and the other a directory, no replacement or integration possible
    if (src_is_dir != dst_is_dir)
      return VITASHELL_ERROR_INVALID_TYPE;

    // Replace file
    if (!src_is_dir && !dst_is_dir && flags & MOVE_REPLACE) {
      sceIoRemove(dst_path);

      res = sceIoRename(src_path, dst_path);
      if (res < 0)
        return res;

      return 1;
    }

    // Integrate directory
    if (src_is_dir && dst_is_dir && flags & MOVE_INTEGRATE) {
      SceUID dfd = sceIoDopen(src_path);
      if (dfd < 0)
        return dfd;

      int res = 0;

      do {
        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));

        res = sceIoDread(dfd, &dir);
        if (res > 0) {
          char *new_src_path = malloc(strlen(src_path) + strlen(dir.d_name) + 2);
          snprintf(new_src_path, MAX_PATH_LENGTH, "%s%s%s", src_path, hasEndSlash(src_path) ? "" : "/", dir.d_name);

          char *new_dst_path = malloc(strlen(dst_path) + strlen(dir.d_name) + 2);
          snprintf(new_dst_path, MAX_PATH_LENGTH, "%s%s%s", dst_path, hasEndSlash(dst_path) ? "" : "/", dir.d_name);

          // Recursive move
          int ret = movePath(new_src_path, new_dst_path, flags, param);

          free(new_dst_path);
          free(new_src_path);

          if (ret <= 0) {
            sceIoDclose(dfd);
            return ret;
          }
        }
      } while (res > 0);

      sceIoDclose(dfd);

      // Integrated, now remove this directory
      sceIoRmdir(src_path);
    }
  }

  return 1;
}
