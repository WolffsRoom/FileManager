#include <sdk_compat.h>
#include <paf.h>
#include "common.h"
#include "dialog.h"
#include "usb_menu.h"

using namespace paf;

namespace {
    void SetText(ui::Widget *widget, const paf::wstring &s)
    {
        widget->SetString(s);
    }
}

UsbMenu::UsbMenu(ui::Widget *parentRoot, usb::Device device) :
    m_parentRoot(parentRoot), m_closing(false), m_stopResult(0)
{
    Plugin::PageOpenParam param;
    param.graphics_flag = 0x80;
    param.overwrite_draw_priority = 8;
    m_root = g_appPlugin->PageOpen(page_ftp, param);
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
}

UsbMenu::~UsbMenu()
{
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
    m_stopResult = usb::Stop();
    m_root->SetActivate(false);
    DeferCall([](void *ptr)
    {
        UsbMenu *menu = static_cast<UsbMenu *>(ptr);
        int32_t result = menu->m_stopResult;
        delete menu;
        if (result < 0) dialog::OpenError(g_appPlugin, result);
    }, this);
}
