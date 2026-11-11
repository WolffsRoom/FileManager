#include <archive.h>
#include <archive_entry.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "legacy/vitashell_error.h"
#include "core/archive_ops.h"

#define ARCHIVE_BUFFER_SIZE (512 * 1024)

static int cancelled(FileProcessParam *param) {
  return param && param->cancelHandler && param->cancelHandler();
}

static void progressed(FileProcessParam *param, uint64_t amount) {
  if (!param) return;
  if (param->value) *param->value += amount;
  if (param->SetProgress) param->SetProgress(param->value ? *param->value : 0, param->max);
}

static int safeArchivePath(const char *path) {
  if (!path || !*path || path[0] == '/' || path[0] == '\\' || strchr(path, ':')) return 0;
  const char *part = path;
  while (*part) {
    while (*part == '/' || *part == '\\') part++;
    const char *end = part;
    while (*end && *end != '/' && *end != '\\') end++;
    if (end - part == 2 && part[0] == '.' && part[1] == '.') return 0;
    part = end;
  }
  return 1;
}

static void normalizeSlashes(char *path) {
  for (; *path; ++path) if (*path == '\\') *path = '/';
}

static void makeParents(char *path) {
  char *first = strchr(path, '/');
  for (char *p = first; p; p = strchr(p + 1, '/')) {
    *p = 0;
    sceIoMkdir(path, 0777);
    *p = '/';
  }
}

int archiveExtract(const char *archivePath, const char *destination, FileProcessParam *param) {
  struct archive *reader = archive_read_new();
  archive_read_support_filter_all(reader);
  archive_read_support_format_all(reader);
  if (archive_read_open_filename(reader, archivePath, ARCHIVE_BUFFER_SIZE) != ARCHIVE_OK) {
    archive_read_free(reader);
    return VITASHELL_ERROR_INVALID_MAGIC;
  }

  sceIoMkdir(destination, 0777);
  void *buffer = malloc(ARCHIVE_BUFFER_SIZE);
  if (!buffer) {
    archive_read_free(reader);
    return VITASHELL_ERROR_NO_MEMORY;
  }

  int result = 1;
  int headerResult = ARCHIVE_OK;
  struct archive_entry *entry;
  while (!cancelled(param) && (headerResult = archive_read_next_header(reader, &entry)) == ARCHIVE_OK) {
    const char *name = archive_entry_pathname(entry);
    if (!safeArchivePath(name)) {
      archive_read_data_skip(reader);
      continue;
    }
    char output[MAX_PATH_LENGTH];
    if (snprintf(output, sizeof(output), "%s/%s", destination, name) >= (int)sizeof(output)) {
      result = VITASHELL_ERROR_INTERNAL;
      break;
    }
    normalizeSlashes(output);
    makeParents(output);

    mode_t type = archive_entry_filetype(entry);
    if (type == AE_IFDIR) {
      sceIoMkdir(output, 0777);
      continue;
    }
    // Vita storage does not have useful symlinks for this workflow. Skipping
    // them also prevents an archive from escaping its destination indirectly.
    if (type != AE_IFREG) {
      archive_read_data_skip(reader);
      continue;
    }

    SceUID fd = sceIoOpen(output, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) { result = fd; break; }
    for (;;) {
      la_ssize_t read = archive_read_data(reader, buffer, ARCHIVE_BUFFER_SIZE);
      if (read < 0) { result = VITASHELL_ERROR_INTERNAL; break; }
      if (read == 0) break;
      int written = sceIoWrite(fd, buffer, (SceSize)read);
      if (written != read) { result = written < 0 ? written : VITASHELL_ERROR_INTERNAL; break; }
      progressed(param, (uint64_t)read);
      if (cancelled(param)) { result = 0; break; }
    }
    sceIoClose(fd);
    if (result <= 0) {
      sceIoRemove(output);
      break;
    }
  }
  if (cancelled(param)) result = 0;
  else if (result > 0 && headerResult < ARCHIVE_WARN) result = VITASHELL_ERROR_INTERNAL;
  free(buffer);
  archive_read_free(reader);
  return result;
}

static const char *baseName(const char *path) {
  const char *slash = strrchr(path, '/');
  return slash && slash[1] ? slash + 1 : path;
}

static int addToZip(struct archive *writer, const char *diskPath, const char *zipPath,
                    void *buffer, FileProcessParam *param) {
  SceIoStat stat;
  memset(&stat, 0, sizeof(stat));
  int res = sceIoGetstat(diskPath, &stat);
  if (res < 0) return res;

  struct archive_entry *entry = archive_entry_new();
  archive_entry_set_pathname(entry, zipPath);
  archive_entry_set_perm(entry, 0777);
  if (SCE_S_ISDIR(stat.st_mode)) {
    archive_entry_set_filetype(entry, AE_IFDIR);
    archive_entry_set_size(entry, 0);
  } else {
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_size(entry, stat.st_size);
  }
  res = archive_write_header(writer, entry);
  archive_entry_free(entry);
  if (res < ARCHIVE_OK) return VITASHELL_ERROR_INTERNAL;

  if (SCE_S_ISDIR(stat.st_mode)) {
    SceUID dir = sceIoDopen(diskPath);
    if (dir < 0) return dir;
    SceIoDirent child;
    while (!cancelled(param)) {
      memset(&child, 0, sizeof(child));
      int count = sceIoDread(dir, &child);
      if (count < 0) { res = count; break; }
      if (count == 0) { res = 1; break; }
      char childDisk[MAX_PATH_LENGTH], childZip[MAX_PATH_LENGTH];
      if (snprintf(childDisk, sizeof(childDisk), "%s/%s", diskPath, child.d_name) >= (int)sizeof(childDisk) ||
          snprintf(childZip, sizeof(childZip), "%s/%s", zipPath, child.d_name) >= (int)sizeof(childZip)) {
        res = VITASHELL_ERROR_INTERNAL; break;
      }
      res = addToZip(writer, childDisk, childZip, buffer, param);
      if (res <= 0) break;
    }
    sceIoDclose(dir);
    return cancelled(param) ? 0 : res;
  }

  SceUID fd = sceIoOpen(diskPath, SCE_O_RDONLY, 0);
  if (fd < 0) return fd;
  res = 1;
  while (!cancelled(param)) {
    int read = sceIoRead(fd, buffer, ARCHIVE_BUFFER_SIZE);
    if (read < 0) { res = read; break; }
    if (read == 0) break;
    la_ssize_t written = archive_write_data(writer, buffer, read);
    if (written != read) { res = VITASHELL_ERROR_INTERNAL; break; }
    progressed(param, (uint64_t)read);
  }
  sceIoClose(fd);
  return cancelled(param) ? 0 : res;
}

int archiveCreateZip(const char *zipPath, const char **sources, int sourceCount, FileProcessParam *param) {
  if (!zipPath || !sources || sourceCount <= 0) return VITASHELL_ERROR_INTERNAL;
  struct archive *writer = archive_write_new();
  archive_write_set_format_zip(writer);
  // Level 1 keeps compression useful while avoiding the large CPU cost of
  // VitaShell/minizip's former default on the Vita's ARM cores.
  archive_write_set_options(writer, "zip:compression=deflate,zip:compression-level=1");
  if (archive_write_open_filename(writer, zipPath) != ARCHIVE_OK) {
    archive_write_free(writer);
    return VITASHELL_ERROR_INTERNAL;
  }
  void *buffer = malloc(ARCHIVE_BUFFER_SIZE);
  if (!buffer) {
    archive_write_close(writer);
    archive_write_free(writer);
    return VITASHELL_ERROR_NO_MEMORY;
  }

  int result = 1;
  for (int i = 0; i < sourceCount && result > 0 && !cancelled(param); ++i)
    result = addToZip(writer, sources[i], baseName(sources[i]), buffer, param);
  if (cancelled(param)) result = 0;
  free(buffer);
  archive_write_close(writer);
  archive_write_free(writer);
  if (result <= 0) sceIoRemove(zipPath);
  return result;
}
