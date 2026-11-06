/*
  VPK installation, from FMVita/VitaShell's package_installer.c (TheFloW, GPLv3),
  with libarchive extraction in place of VitaShell's archive layer.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <archive.h>
#include <archive_entry.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/sysmodule.h>
#include <psp2/promoterutil.h>

#include "legacy/vitashell_error.h"
#include "legacy/sha1.h"
#include "core/install.h"

#define HEAD_BIN FM_PACKAGE_DIR "/sce_sys/package/head.bin"
#define HEAD_BIN_TEMPLATE "app0:resource/head.bin"

typedef struct {
  uint32_t magic, version, keyofs, valofs, count;
} __attribute__((packed)) SfoHeader;

typedef struct {
  uint16_t nameofs;
  uint8_t alignment, type;
  uint32_t valsize, totalsize, dataofs;
} __attribute__((packed)) SfoEntry;

static int readWhole(const char *file, void **buffer) {
  SceUID fd = sceIoOpen(file, SCE_O_RDONLY, 0);
  if (fd < 0)
    return fd;
  int size = sceIoLseek32(fd, 0, SCE_SEEK_END);
  sceIoLseek32(fd, 0, SCE_SEEK_SET);
  *buffer = malloc(size);
  if (!*buffer) {
    sceIoClose(fd);
    return VITASHELL_ERROR_NO_MEMORY;
  }
  int read = sceIoRead(fd, *buffer, size);
  sceIoClose(fd);
  return read;
}

static int sfoString(void *buffer, const char *name, char *string, int length) {
  SfoHeader *header = (SfoHeader *)buffer;
  SfoEntry *entries = (SfoEntry *)((uint8_t *)buffer + sizeof(SfoHeader));
  if (header->magic != 0x46535000)
    return VITASHELL_ERROR_INVALID_MAGIC;
  for (uint32_t i = 0; i < header->count; i++) {
    if (strcmp((char *)buffer + header->keyofs + entries[i].nameofs, name) == 0) {
      memset(string, 0, length);
      strncpy(string, (char *)buffer + header->valofs + entries[i].dataofs, length);
      string[length - 1] = '\0';
      return 0;
    }
  }
  return VITASHELL_ERROR_NOT_FOUND;
}

static uint32_t be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void fpkg_hmac(const uint8_t *data, unsigned int len, uint8_t hmac[16]) {
  SHA1_CTX ctx;
  uint8_t sha1[20];
  uint8_t buf[64];

  sha1_init(&ctx);
  sha1_update(&ctx, data, len);
  sha1_final(&ctx, sha1);

  memset(buf, 0, 64);
  memcpy(&buf[0], &sha1[4], 8);
  memcpy(&buf[8], &sha1[4], 8);
  memcpy(&buf[16], &sha1[12], 4);
  buf[20] = sha1[16];
  buf[21] = sha1[1];
  buf[22] = sha1[2];
  buf[23] = sha1[3];
  memcpy(&buf[24], &buf[16], 8);

  sha1_init(&ctx);
  sha1_update(&ctx, buf, 64);
  sha1_final(&ctx, sha1);
  memcpy(hmac, sha1, 16);
}

static int makeHeadBin(char *titleid_out) {
  uint8_t hmac[16];
  uint32_t off, len, out;

  void *sfo = NULL;
  int res = readWhole(FM_PACKAGE_DIR "/sce_sys/param.sfo", &sfo);
  if (res < 0)
    return res;

  char titleid[12];
  memset(titleid, 0, sizeof(titleid));
  sfoString(sfo, "TITLE_ID", titleid, sizeof(titleid));
  char contentid[48];
  memset(contentid, 0, sizeof(contentid));
  sfoString(sfo, "CONTENT_ID", contentid, sizeof(contentid));
  free(sfo);

  if (strlen(titleid) != 9)
    return VITASHELL_ERROR_INVALID_TITLEID;
  for (int i = 0; i < 9; i++)
    if (islower((unsigned char)titleid[i]))
      return VITASHELL_ERROR_INVALID_TITLEID;
  strcpy(titleid_out, titleid);

  SceIoStat stat;
  if (sceIoGetstat(HEAD_BIN, &stat) >= 0)
    return 0;

  uint8_t *head = NULL;
  int size = readWhole(HEAD_BIN_TEMPLATE, (void **)&head);
  if (size < 0)
    return size;

  char full_title_id[48];
  snprintf(full_title_id, sizeof(full_title_id), "EP9000-%s_00-0000000000000000", titleid);
  strncpy((char *)&head[0x30], strlen(contentid) > 0 ? contentid : full_title_id, 48);

  len = be32(&head[0xD0]);
  fpkg_hmac(&head[0], len, hmac);
  memcpy(&head[len], hmac, 16);

  off = be32(&head[0x8]);
  len = be32(&head[0x10]);
  out = be32(&head[0xD4]);
  fpkg_hmac(&head[off], len - 64, hmac);
  memcpy(&head[out], hmac, 16);

  len = be32(&head[0xE8]);
  fpkg_hmac(&head[0], len, hmac);
  memcpy(&head[len], hmac, 16);

  sceIoMkdir(FM_PACKAGE_DIR "/sce_sys/package", 0777);
  SceUID fd = sceIoOpen(HEAD_BIN, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
  if (fd >= 0) {
    sceIoWrite(fd, head, size);
    sceIoClose(fd);
  }
  free(head);
  return fd < 0 ? fd : 0;
}

static int promoteApp(const char *path) {
  // ScePaf is already loaded by this application, which ScePromoterUtil needs.
  int res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
  if (res < 0)
    return res;
  res = scePromoterUtilityInit();
  if (res < 0)
    return res;
  int promote = scePromoterUtilityPromotePkgWithRif(path, 1);
  scePromoterUtilityExit();
  sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
  return promote;
}

static void makeParents(char *path) {
  for (char *p = strchr(path, '/'); p; p = strchr(p + 1, '/')) {
    *p = 0;
    sceIoMkdir(path, 0777);
    *p = '/';
  }
}

int readVpkInfo(const char *vpk, VpkInfo *info) {
  struct archive *a = archive_read_new();
  archive_read_support_format_zip(a);
  if (archive_read_open_filename(a, vpk, 64 * 1024) != ARCHIVE_OK) {
    archive_read_free(a);
    return VITASHELL_ERROR_INVALID_MAGIC;
  }
  int found = 0;
  struct archive_entry *e;
  while (found < 2 && archive_read_next_header(a, &e) == ARCHIVE_OK) {
    const char *name = archive_entry_pathname(e);
    int isSfo = name && strcmp(name, "sce_sys/param.sfo") == 0;
    int isIcon = name && strcmp(name, "sce_sys/icon0.png") == 0;
    if (!isSfo && !isIcon)
      continue;
    int64_t size = archive_entry_size(e);
    if (size <= 0 || size > 4 * 1024 * 1024)
      continue;
    uint8_t *buf = malloc(size);
    if (!buf)
      break;
    if (archive_read_data(a, buf, size) != size) {
      free(buf);
      continue;
    }
    found++;
    if (isSfo) {
      sfoString(buf, "TITLE", info->title, sizeof(info->title));
      sfoString(buf, "APP_VER", info->version, sizeof(info->version));
      sfoString(buf, "TITLE_ID", info->titleid, sizeof(info->titleid));
      free(buf);
    } else {
      info->icon = buf;
      info->iconSize = (uint32_t)size;
    }
  }
  archive_read_free(a);
  return found ? 0 : VITASHELL_ERROR_NOT_FOUND;
}

int64_t archiveTotalSize(const char *path) {
  struct archive *a = archive_read_new();
  archive_read_support_format_all(a);
  archive_read_support_filter_all(a);
  if (archive_read_open_filename(a, path, 64 * 1024) != ARCHIVE_OK) {
    archive_read_free(a);
    return VITASHELL_ERROR_INVALID_MAGIC;
  }
  int64_t total = 0;
  struct archive_entry *e;
  while (archive_read_next_header(a, &e) == ARCHIVE_OK)
    total += archive_entry_size(e);
  archive_read_free(a);
  return total;
}

static int extract(const char *archive_path, const char *dest, FileProcessParam *param) {
  struct archive *a = archive_read_new();
  archive_read_support_format_all(a);
  archive_read_support_filter_all(a);
  if (archive_read_open_filename(a, archive_path, 64 * 1024) != ARCHIVE_OK) {
    archive_read_free(a);
    return VITASHELL_ERROR_INVALID_MAGIC;
  }

  void *buf = malloc(TRANSFER_SIZE);
  int result = 1;
  struct archive_entry *e;
  char out[MAX_PATH_LENGTH];

  while (result > 0 && archive_read_next_header(a, &e) == ARCHIVE_OK) {
    const char *name = archive_entry_pathname(e);
    if (!name || strstr(name, ".."))
      continue;
    snprintf(out, sizeof(out), "%s/%s", dest, name);

    if (archive_entry_filetype(e) == AE_IFDIR) {
      makeParents(out);
      sceIoMkdir(out, 0777);
      continue;
    }

    makeParents(out);
    SceUID fd = sceIoOpen(out, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) {
      result = fd;
      break;
    }
    for (;;) {
      la_ssize_t n = archive_read_data(a, buf, TRANSFER_SIZE);
      if (n < 0) {
        result = VITASHELL_ERROR_INTERNAL;
        break;
      }
      if (n == 0)
        break;
      int w = sceIoWrite(fd, buf, n);
      if (w < 0) {
        result = w;
        break;
      }
      if (param) {
        if (param->value)
          *param->value += n;
        if (param->SetProgress)
          param->SetProgress(param->value ? *param->value : 0, param->max);
        if (param->cancelHandler && param->cancelHandler()) {
          result = 0;
          break;
        }
      }
    }
    sceIoClose(fd);
  }

  free(buf);
  archive_read_free(a);
  return result;
}

int installVpk(const char *vpk, FileProcessParam *param, char *titleid) {
  removePath(FM_PACKAGE_DIR, NULL);
  sceIoMkdir(FM_PACKAGE_DIR, 0777);

  int res = extract(vpk, FM_PACKAGE_DIR, param);
  if (res <= 0) {
    removePath(FM_PACKAGE_DIR, NULL);
    return res;
  }

  res = makeHeadBin(titleid);
  if (res < 0) {
    removePath(FM_PACKAGE_DIR, NULL);
    return res;
  }

  res = promoteApp(FM_PACKAGE_DIR);
  // The promoter moves the files away; whatever is left is debris.
  removePath(FM_PACKAGE_DIR, NULL);
  return res < 0 ? res : 1;
}
