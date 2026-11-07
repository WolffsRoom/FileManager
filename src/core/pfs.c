/* PFS mounting derived from VitaShell/FMVita, Copyright (C) TheFloW. */
#include <psp2/appmgr.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>

#include "../../modules/user/vitashell_user.h"
#include "pfs.h"

#define FM_TITLEID "FILEMGR01"

static char s_mountPoint[16];
static SceUID s_kernelMod = -1;
static SceUID s_userMod = -1;

/* Keep the original FMVita / VitaShell user module bridge, not a guessed
   decryption of encrypted files. A decrypted view exists only after AppMgr
   successfully mounts a protected container. */
typedef struct {
    size_t size;
    SceUID pid;
    size_t args;
    void *argp;
    int flags;
} FmTaiModuleArgs;
extern SceUID taiLoadStartKernelModuleForUser(const char *path, FmTaiModuleArgs *args);

static void pfsLog(const char *step, const char *path, int32_t result)
{
    SceUID fd = sceIoOpen("ux0:data/FileManager/pfs.log",
        SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
    if (fd < 0) return;
    char line[340];
    int n = snprintf(line, sizeof(line), "%s [%s] -> 0x%08X mount=%s\n",
        step ? step : "pfs", path ? path : "",
        (unsigned int)result, s_mountPoint);
    if (n > 0) sceIoWrite(fd, line, n < (int)sizeof(line) ? n : (int)sizeof(line) - 1);
    sceIoClose(fd);
}

int pfsInitModules(void)
{
    if (s_userMod >= 0) return 0;

    FmTaiModuleArgs moduleArgs;
    memset(&moduleArgs, 0, sizeof(moduleArgs));
    moduleArgs.size = sizeof(moduleArgs);
    s_kernelMod = taiLoadStartKernelModuleForUser("app0:module/kernel.skprx", &moduleArgs);
    pfsLog("kernel load", NULL, s_kernelMod);
    // A negative result here can also mean the kernel module is already
    // resident. The user module may still bind its existing export.
    s_userMod = sceKernelLoadStartModule("app0:module/user.suprx", 0, NULL, 0, NULL, NULL);
    pfsLog("user load", NULL, s_userMod);
    return s_userMod < 0 ? s_userMod : 0;
}

static int pfsVerifyMount(void)
{
    if (!s_mountPoint[0]) return -1;
    SceUID fd = sceIoDopen(s_mountPoint);
    if (fd < 0) return fd;
    SceIoDirent ent;
    memset(&ent, 0, sizeof(ent));
    int ret = sceIoDread(fd, &ent);
    sceIoDclose(fd);
    // Some mount IDs return success while exposing no files. Reject these
    // instead of showing an empty/undecrypted folder as a success.
    return ret > 0 ? 0 : -1;
}

static int pfsTryMountId(const char *path, ShellMountIdArgs *args, int id)
{
    args->id = id;
    memset(s_mountPoint, 0, sizeof(s_mountPoint));
    int32_t res = shellUserMountById(args);
    char step[40];
    snprintf(step, sizeof(step), "mount-id-0x%X", (unsigned)id);
    pfsLog(step, path, res);
    if (res < 0) return res;
    int verified = pfsVerifyMount();
    pfsLog("verify-id", path, verified);
    if (verified >= 0) return res;
    if (s_mountPoint[0]) sceAppMgrUmount(s_mountPoint);
    memset(s_mountPoint, 0, sizeof(s_mountPoint));
    return verified;
}

int pfsMount(const char *path)
{
    if (!path || !path[0]) return -1;
    if (s_mountPoint[0])
    {
        pfsLog("mount-busy", path, -1);
        return -1; // a previous encrypted view has not been closed yet
    }
    int res = pfsInitModules();
    if (res < 0) return res;

    char klicensee[0x10] = { 0 };
    ShellMountIdArgs args;
    memset(&args, 0, sizeof(args));
    args.process_titleid = FM_TITLEID;
    args.path = path;
    args.desired_mount_point = NULL;
    args.klicensee = klicensee;
    args.mount_point = s_mountPoint;

    // VitaShell known_pfs_ids, in its original order. Avoid the broad
    // 0x3E8/0x3E9/0x3EA guessing that could falsely "mount" an empty view.
    static const int knownIds[] = { 0x6E, 0x12E, 0x12F, 0x3ED };
    int lastError = -1;
    for (unsigned i = 0; i < sizeof(knownIds) / sizeof(knownIds[0]); ++i)
    {
        res = pfsTryMountId(path, &args, knownIds[i]);
        if (res >= 0) return res;
        lastError = res;
    }

    // Same read-only fallback used in VitaShell for game data / DLC.
    memset(s_mountPoint, 0, sizeof(s_mountPoint));
    res = sceAppMgrGameDataMount(path, 0, 0, s_mountPoint);
    pfsLog("gamedata-fallback", path, res);
    if (res >= 0)
    {
        int verified = pfsVerifyMount();
        pfsLog("verify-gamedata", path, verified);
        if (verified >= 0) return res;
        if (s_mountPoint[0]) sceAppMgrUmount(s_mountPoint);
        memset(s_mountPoint, 0, sizeof(s_mountPoint));
        return verified;
    }
    memset(s_mountPoint, 0, sizeof(s_mountPoint));
    return res < 0 ? res : lastError;
}

int pfsUmount(void)
{
    if (!s_mountPoint[0]) return 0;
    int res = sceAppMgrUmount(s_mountPoint);
    pfsLog("umount", NULL, res);
    if (res >= 0) memset(s_mountPoint, 0, sizeof(s_mountPoint));
    return res;
}

const char *pfsMountPoint(void)
{
    return s_mountPoint;
}

int pfsIsProtectedContainer(const char *path)
{
	static const char *containers[] = {
		"ux0:app", "gro0:app", "vs0:app", "ux0:patch", "grw0:patch",
		"ux0:addcont", "ux0:appmeta", "ux0:user/00/savedata",
		"ur0:user/00/savedata", "ur0:user/00/trophy/data"
	};
	if (!path)
		return 0;
	for (unsigned i = 0; i < sizeof(containers) / sizeof(containers[0]); i++)
	{
		if (!strcasecmp(path, containers[i]))
			return 1;
	}
	return 0;
}

int pfsShouldMount(const char *path)
{
	if (!path || !path[0])
		return 0;
	char marker[1024];
	SceIoStat stat;
	snprintf(marker, sizeof(marker), "%s/sce_pfs", path);
	memset(&stat, 0, sizeof(stat));
	return sceIoGetstat(marker, &stat) >= 0 && SCE_S_ISDIR(stat.st_mode);
}
