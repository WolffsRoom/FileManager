#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/sysmodule.h>
#include <psp2/promoterutil.h>

#define STATUS_DIR   "ux0:data/FileManager"
#define STATUS_FILE  STATUS_DIR "/bginstall.status"
#define COMMAND_FILE STATUS_DIR "/bginstall.command"
#define READY_FILE   STATUS_DIR "/bginstall.ready"
#define PACKAGE_DIR  "ux0:data/pkg"

SceUID __real_sceIoOpen(const char *file, int flags, SceMode mode);
int __real_sceIoWrite(SceUID fd, const void *data, SceSize size);
int __real_sceIoClose(SceUID fd);
SceUID __wrap_sceIoOpen(const char *file, int flags, SceMode mode) { return __real_sceIoOpen(file, flags, mode); }
int __wrap_sceIoWrite(SceUID fd, const void *data, SceSize size) { return __real_sceIoWrite(fd, data, size); }
int __wrap_sceIoClose(SceUID fd) { return __real_sceIoClose(fd); }

static void touch_file(const char *path)
{
  SceUID fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
  if (fd >= 0) sceIoClose(fd);
}

static void write_status(const char *state, int result)
{
  char text[96];
  int length = sceClibSnprintf(text, sizeof(text), "state=%s\nresult=%d\n", state, result);
  SceUID fd = sceIoOpen(STATUS_FILE, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
  if (fd >= 0) {
    if (length > 0) sceIoWrite(fd, text, length);
    sceIoClose(fd);
  }
}

static int promote_thread(SceSize args, void *argp)
{
  (void)args;
  (void)argp;
  touch_file(READY_FILE);
  for (;;) {
    SceIoStat stat;
    sceClibMemset(&stat, 0, sizeof(stat));
    if (sceIoGetstat(READY_FILE, &stat) < 0) touch_file(READY_FILE);
    sceClibMemset(&stat, 0, sizeof(stat));
    if (sceIoGetstat(COMMAND_FILE, &stat) < 0) {
      sceKernelDelayThread(250000);
      continue;
    }
    sceIoRemove(COMMAND_FILE);
    write_status("promoting", 0);
    int result = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    if (result >= 0) result = scePromoterUtilityInit();
    if (result >= 0) result = scePromoterUtilityPromotePkgWithRif(PACKAGE_DIR, 1);
    scePromoterUtilityExit();
    sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    write_status(result < 0 ? "error" : "done", result);
  }
  return sceKernelExitDeleteThread(0);
}

int module_start(SceSize args, void *argp)
{
  (void)args;
  (void)argp;
  sceIoMkdir(STATUS_DIR, 0777);
  SceUID thread = sceKernelCreateThread("FM::BackgroundPromoter", promote_thread,
    0x10000100, 0x8000, 0, 0, 0);
  if (thread < 0 || sceKernelStartThread(thread, 0, 0) < 0)
    return SCE_KERNEL_START_FAILED;
  return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, void *argp)
{
  (void)args;
  (void)argp;
  return SCE_KERNEL_STOP_SUCCESS;
}
