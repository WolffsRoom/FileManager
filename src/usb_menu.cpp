#include <sdk_compat.h>
#include <paf.h>
#include "common.h"
#include "dialog.h"
#include "usb_menu.h"
#include <new>

using namespace paf;

namespace {
    void SetText(ui::Widget *widget, const paf::wstring &s)
    {
        widget->SetString(s);
    }
}

bool UsbMenu::Open(ui::Widget *parentRoot, usb::Device device)
{
	UsbMenu *menu = new (std::nothrow) UsbMenu(parentRoot, device);
	if (!menu) return false;
	if (!menu->m_root)
	{
		delete menu;
		return false;
	}
	return true;
}

UsbMenu::UsbMenu(ui::Widget *parentRoot, usb::Device device) :
    m_parentRoot(parentRoot), m_root(NULL), m_closing(false), m_stopResult(0), m_startResult(0), m_device(device)
{
    Plugin::PageOpenParam param;
    param.graphics_flag = 0x80;
    param.overwrite_draw_priority = 8;
    m_root = g_appPlugin->PageOpen(page_ftp, param);
    if (!m_root) return;
    m_parentRoot->SetActivate(false);

    static const uint32_t k_names[] = {
        msg_usb_memory_card, msg_usb_game_card, msg_usb_sd2vita, msg_usb_psvsd
    };
    int selected = (device >= usb::Device_MemoryCard && device < usb::Device_Count) ? device : 0;
    SetText(m_root->FindChild(text_title_page_ftp), g_appPlugin->GetString(msg_usb));
    SetText(m_root->FindChild(text_address_page_ftp), g_appPlugin->GetString(k_names[selected]));
    SetText(m_root->FindChild(text_connection_page_ftp), g_appPlugin->GetString(msg_usb_connected));
    SetText(m_root->FindChild(text_activity_page_ftp), g_appPlugin->GetString(msg_usb_eject));
    SetText(m_root->FindChild(text_progress_page_ftp), paf::wstring());
    m_root->FindChild(plane_bar_page_ftp)->Hide(common::transition::Type_Reset);

    ui::Widget *stop = m_root->FindChild(button_ok_page_ftp);
    SetText(stop, g_appPlugin->GetString(msg_usb_disconnect));
    stop->SetKeycode(inputdevice::pad::Data::PAD_SELECT);
    stop->AddEventCallback(ui::Button::CB_BTN_DECIDE,
        [](int32_t, ui::Handler *, ui::Event *, void *userdata)
        {
            static_cast<UsbMenu *>(userdata)->Disconnect();
        }, this);
    ui::Widget *back = m_root->FindChild(button_cancel_page_ftp);
    SetText(back, g_appPlugin->GetString(msg_cancel));
    back->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE);
    back->AddEventCallback(ui::Button::CB_BTN_DECIDE,
        [](int32_t, ui::Handler *, ui::Event *, void *userdata)
        {
            static_cast<UsbMenu *>(userdata)->Disconnect();
        }, this);

    // Never start a kernel USB session inside a button/PAF widget callback.
    // The next-frame task runs on the actual PAF main thread.
    SetText(m_root->FindChild(text_connection_page_ftp),
        g_appPlugin->GetString(msg_usb_starting));
    DeferCall([](void *userdata)
    {
        static_cast<UsbMenu *>(userdata)->StartConnection();
    }, this);
}

void UsbMenu::StartConnection()
{
    if (m_closing || !m_root) return;
    m_startResult = usb::Toggle(m_device);
    if (m_startResult < 0)
    {
        SetText(m_root->FindChild(text_connection_page_ftp),
            g_appPlugin->GetString(msg_usb_failed));
        dialog::OpenError(g_appPlugin, m_startResult);
    }
    else
        SetText(m_root->FindChild(text_connection_page_ftp),
            g_appPlugin->GetString(msg_usb_connected));
}

UsbMenu::~UsbMenu()
{
    if (!m_root) return;
    Plugin::PageCloseParam closeParam;
    closeParam.fade = true;
    closeParam.fade_time_ms = 200.0f;
    g_appPlugin->PageClose(page_ftp, closeParam);
    m_parentRoot->SetActivate(true);
}

void UsbMenu::Disconnect()
{
    if (m_closing) return;
    m_closing = true;
    m_root->SetActivate(false);
    DeferCall([](void *ptr)
    {
        UsbMenu *menu = static_cast<UsbMenu *>(ptr);
        // Stop the mass-storage service outside the PAF button callback.
        const int32_t result = usb::Stop();
        delete menu;
        if (result < 0) dialog::OpenError(g_appPlugin, result);
    }, this);
}
