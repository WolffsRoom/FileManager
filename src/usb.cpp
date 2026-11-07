/* USB mass-storage flow adapted from VitaShell/FMVita by TheFloW. */
#include <sdk_compat.h>
#include <psp2/io/stat.h>
// The project's PAF compatibility layer has a minimal taihen.h shim.
// Use the official user-mode taiHEN syscall ABI for kernel USB modules.
#include <psp2kern/types.h>
struct UsbTaiModuleArgs {
	size_t size;
	SceUID pid;
	size_t args;
	void *argp;
	int flags;
};
extern "C" SceUID taiLoadStartKernelModuleForUser(const char *path, UsbTaiModuleArgs *args);
extern "C" int taiStopUnloadKernelModuleForUser(SceUID modid, UsbTaiModuleArgs *args, void *opt, int *res);

#include "usb.h"

extern "C" int sceMtpIfStopDriver(int mode);
extern "C" int sceMtpIfStartDriver(int mode);
extern "C" int sceUsbstorVStorSetDeviceInfo(const char *name, const char *version);
extern "C" int sceUsbstorVStorSetImgFilePath(const char *path);
extern "C" int sceUsbstorVStorStart(int type);
extern "C" int sceUsbstorVStorStop(void);
extern "C" int vshIoUmount(int id, int mode, int zero1, int zero2);

namespace {
	SceUID s_module = -1;
	const int kFatDevice = 1;

	bool Exists(const char *path)
	{
		SceIoStat stat;
		return sceIoGetstat(path, &stat) >= 0;
	}

	void Remount(int id)
	{
		vshIoUmount(id, 0, 0, 0);
		vshIoUmount(id, 1, 0, 0);
	}
}

const char *usb::DevicePath(Device device)
{
	switch (device)
	{
	case Device_MemoryCard:
		if (Exists("sdstor0:xmc-lp-ign-userext")) return "sdstor0:xmc-lp-ign-userext";
		if (Exists("sdstor0:int-lp-ign-userext")) return "sdstor0:int-lp-ign-userext";
		break;
	case Device_GameCard: if (Exists("sdstor0:gcd-lp-ign-gamero")) return "sdstor0:gcd-lp-ign-gamero"; break;
	case Device_SD2Vita: if (Exists("sdstor0:gcd-lp-ign-entire")) return "sdstor0:gcd-lp-ign-entire"; break;
	case Device_PSVSD:
		if (Exists("sdstor0:uma-pp-act-a")) return "sdstor0:uma-pp-act-a";
		if (Exists("sdstor0:uma-lp-act-entire")) return "sdstor0:uma-lp-act-entire";
		break;
	default: break;
	}
	return NULL;
}

bool usb::IsActive() { return s_module >= 0; }

int32_t usb::Stop()
{
	if (s_module < 0) return 0;
	int32_t first = sceUsbstorVStorStop();
	int32_t mtp = sceMtpIfStartDriver(1);
	UsbTaiModuleArgs args = { sizeof(UsbTaiModuleArgs), 0, 0, NULL, 0 };
	int32_t unload = taiStopUnloadKernelModuleForUser(s_module, &args, NULL, NULL);
	s_module = -1;
	Remount(0x800);
	if (Exists("imc0:")) Remount(0xD00);
	if (Exists("xmc0:")) Remount(0xE00);
	if (Exists("uma0:")) Remount(0xF00);
	if (first < 0) return first;
	if (mtp < 0) return mtp;
	return unload;
}

int32_t usb::Toggle(Device device)
{
	if (IsActive()) return Stop();
	const char *path = DevicePath(device);
	if (!path) return (int32_t)0x80010013; // ENODEV
	UsbTaiModuleArgs args = { sizeof(UsbTaiModuleArgs), 0, 0, NULL, 0 };
	int32_t res = taiLoadStartKernelModuleForUser("app0:module/usbdevice.skprx", &args);
	if (res < 0) return res;
	s_module = res;
	res = sceMtpIfStopDriver(1);
	if (res < 0 && (uint32_t)res != 0x8054360C) { Stop(); return res; }
	res = sceUsbstorVStorSetDeviceInfo("File Manager USB", "0.40");
	if (res >= 0) res = sceUsbstorVStorSetImgFilePath(path);
	if (res >= 0) res = sceUsbstorVStorStart(kFatDevice);
	if (res < 0) { Stop(); return res; }
	return 0;
}
