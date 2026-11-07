#ifndef _FM_USB_H_
#define _FM_USB_H_

#include <sdk_compat.h>

namespace usb {
	enum Device { Device_MemoryCard = 0, Device_GameCard, Device_SD2Vita, Device_PSVSD, Device_Count };
	int32_t Toggle(Device device);
	int32_t Stop();
	bool IsActive();
	const char *DevicePath(Device device);
}

#endif
