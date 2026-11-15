/* USB mass-storage flow adapted from VitaShell/FMVita by TheFloW. */
#include <sdk_compat.h>
#include <psp2/io/stat.h>
#include <psp2/appmgr.h>
#include <stdio.h>
#include "settings.h"
#include <taihen.h>

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
	bool s_storageStarted = false;
	bool s_mtpStopped = false;
	const int kFatDevice = 1;

	void UsbLog(const char *step, int32_t result)
	{
		SceUID fd = sceIoOpen(FM_DATA_DIR "/usb.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
		if (fd < 0) return;
		char text[112];
		const int len = snprintf(text, sizeof(text), "%s: 0x%08X\n", step, (uint32_t)result);
		if (len > 0) sceIoWrite(fd, text, len);
		sceIoClose(fd);
	}

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
		if (Exists("xmc0:")) return "sdstor0:xmc-lp-ign-userext";
		return "sdstor0:int-lp-ign-userext";
		break;
	case Device_GameCard: return "sdstor0:gcd-lp-ign-gamero";
	case Device_SD2Vita: return "sdstor0:gcd-lp-ign-entire";
	case Device_PSVSD:
		return "sdstor0:uma-pp-act-a";
		break;
	default: break;
	}
	return NULL;
}

bool usb::IsActive() { return s_module >= 0; }

int32_t usb::Stop()
{
	if (s_module < 0) return 0;
	int32_t result = 0;
	if (s_storageStarted)
	{
		int32_t rc = sceUsbstorVStorStop();
		UsbLog("VStorStop", rc);
		if (rc < 0) result = rc;
		s_storageStarted = false;
	}
	if (s_mtpStopped)
	{
		int32_t rc = sceMtpIfStartDriver(1);
		UsbLog("MtpStart", rc);
		if (rc < 0 && result >= 0) result = rc;
		s_mtpStopped = false;
	}
	int32_t rc = taiStopUnloadKernelModule(s_module, 0, NULL, 0, NULL, NULL);
	UsbLog("UnloadKernelModule", rc);
	if (rc < 0 && result >= 0) result = rc;
	s_module = -1;
	Remount(0x800);
	if (Exists("imc0:")) Remount(0xD00);
	if (Exists("xmc0:")) Remount(0xE00);
	if (Exists("uma0:")) Remount(0xF00);
	return result;
}

int32_t usb::Toggle(Device device)
{
	if (IsActive()) return Stop();
	const char *path = DevicePath(device);
	if (!path)
	{
		UsbLog("DevicePathUnavailable", (int32_t)0x80010013);
		return (int32_t)0x80010013;
	}
	// Mirror VitaShell's preparation before exposing a filesystem to the PC.
	int32_t otherApps = sceAppMgrDestroyOtherApp();
	UsbLog("DestroyOtherApps", otherApps);
	int32_t res = taiLoadStartKernelModule("app0:module/usbdevice.skprx", 0, NULL, 0);
	UsbLog("LoadKernelModule", res);
	if (res < 0) return res;
	s_module = res;
	res = sceMtpIfStopDriver(1);
	UsbLog("MtpStop", res);
	if (res >= 0) s_mtpStopped = true;
	else if ((uint32_t)res != 0x8054360C)
	{
		const int32_t error = res;
		Stop();
		return error;
	}
	res = sceUsbstorVStorSetDeviceInfo("File Manager USB", "0.80");
	UsbLog("DeviceInfo", res);
	if (res >= 0)
	{
		res = sceUsbstorVStorSetImgFilePath(path);
		UsbLog("ImagePath", res);
	}
	if (res >= 0)
	{
		res = sceUsbstorVStorStart(kFatDevice);
		UsbLog("VStorStart", res);
	}
	if (res < 0)
	{
		const int32_t error = res;
		Stop();
		return error;
	}
	s_storageStarted = true;
	UsbLog("USB_ACTIVE", 0);
	return 0;
}
