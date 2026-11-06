/* PFS mounting derived from VitaShell/FMVita, Copyright (C) TheFloW. */
#include <psp2/appmgr.h>
#include <psp2/kernel/modulemgr.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "../../modules/user/vitashell_user.h"
#include "pfs.h"

#define FM_TITLEID "FILEMGR01"

static char s_mountPoint[16];
static SceUID s_kernelMod = -1;
static SceUID s_userMod = -1;

/* The project has a tiny ScePaf taihen compatibility header, so declare the
   real loader syscall used by VitaShell explicitly here. */
typedef struct {
	size_t size;
	SceUID pid;
	size_t args;
	void *argp;
	int flags;
} FmTaiModuleArgs;
extern SceUID taiLoadStartKernelModuleForUser(const char *path, FmTaiModuleArgs *args);

int pfsInitModules(void)
{
	if (s_userMod >= 0)
		return 0;

	/* If VitaShellKernel2 is already resident, loading it simply fails and the
	   user bridge binds to the existing exported library. */
	FmTaiModuleArgs moduleArgs;
	memset(&moduleArgs, 0, sizeof(moduleArgs));
	moduleArgs.size = sizeof(moduleArgs);
	s_kernelMod = taiLoadStartKernelModuleForUser("app0:module/kernel.skprx", &moduleArgs);
	s_userMod = sceKernelLoadStartModule("app0:module/user.suprx", 0, NULL, 0, NULL, NULL);
	return s_userMod < 0 ? s_userMod : 0;
}

int pfsMount(const char *path)
{
	static const int ids[] = { 0x6E, 0x12E, 0x12F, 0x3ED };
	char klicensee[0x10];
	ShellMountIdArgs args;
	int res = pfsInitModules();
	if (res < 0)
		return res;

	memset(klicensee, 0, sizeof(klicensee));
	memset(s_mountPoint, 0, sizeof(s_mountPoint));
	memset(&args, 0, sizeof(args));
	args.process_titleid = FM_TITLEID;
	args.path = path;
	args.klicensee = klicensee;
	args.mount_point = s_mountPoint;

	for (unsigned i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
	{
		args.id = ids[i];
		res = shellUserMountById(&args);
		if (res >= 0)
			return res;
	}

	return sceAppMgrGameDataMount(path, 0, 0, s_mountPoint);
}

int pfsUmount(void)
{
	if (!s_mountPoint[0])
		return 0;
	int res = sceAppMgrUmount(s_mountPoint);
	if (res >= 0)
		memset(s_mountPoint, 0, sizeof(s_mountPoint));
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
