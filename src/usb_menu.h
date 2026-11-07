#ifndef _FM_USB_MENU_H_
#define _FM_USB_MENU_H_

#include <sdk_compat.h>
#include <paf.h>
#include "usb.h"

/** Non-backgroundable USB mass-storage session to avoid simultaneous Vita FS writes. */
class UsbMenu
{
public:
    static bool Open(paf::ui::Widget *parentRoot, usb::Device device);
    UsbMenu(paf::ui::Widget *parentRoot, usb::Device device);
    ~UsbMenu();

private:
    void Disconnect();
    paf::ui::Widget *m_parentRoot;
    paf::ui::Scene *m_root;
    bool m_closing;
    int32_t m_stopResult;
    int32_t m_startResult;
};
#endif
