#include <sdk_compat.h>
#include <paf.h>

#include "common.h"
#include "settings.h"
#include "notice_menu.h"

using namespace paf;

namespace {

	void SetString(ui::Widget *w, uint32_t id)
	{
		// Through UTF-8 so the PAF builds the string itself.
		paf::string s;
		common::Utf16ToUtf8(paf::wstring(g_appPlugin->GetString(id)), &s);
		paf::wstring text;
		common::Utf8ToUtf16(s, &text);
		w->SetString(text);
	}
}

NoticeMenu::NoticeMenu(ui::Widget *parentRoot, uint32_t title, uint32_t message, uint32_t primary, uint32_t secondary, ResultCallback cb, void *userdata) :
	m_parentRoot(parentRoot), m_cb(cb), m_userdata(userdata), m_choice(1)
{
	Plugin::PageOpenParam openParam;
	openParam.graphics_flag = 0x80;
	openParam.overwrite_draw_priority = 8;
	m_root = g_appPlugin->PageOpen(page_notice, openParam);
	m_parentRoot->SetActivate(false);

	SetString(m_root->FindChild(text_title_page_notice), title);
	SetString(m_root->FindChild(text_message_page_notice), message);

	ui::Widget *first = m_root->FindChild(button_primary_page_notice);
	ui::Widget *second = m_root->FindChild(button_secondary_page_notice);
	SetString(first, primary);
	if (secondary)
	{
		SetString(second, secondary);
		second->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE);
	}
	else
	{
		second->Hide(common::transition::Type_Reset);
		first->SetPos(0.0f, -100.0f, 0.0f);
		first->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE);
	}

	first->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<NoticeMenu *>(userdata)->Close(0);
	}, this);
	second->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<NoticeMenu *>(userdata)->Close(1);
	}, this);
}

NoticeMenu::~NoticeMenu()
{
	Plugin::PageCloseParam closeParam;
	closeParam.fade = true;
	closeParam.fade_time_ms = 200.0f;
	g_appPlugin->PageClose(page_notice, closeParam);
	m_parentRoot->SetActivate(true);
}

void NoticeMenu::Close(int32_t choice)
{
	m_root->SetActivate(false);
	m_choice = choice;
	DeferCall([](void *p)
	{
		NoticeMenu *menu = static_cast<NoticeMenu *>(p);
		ResultCallback cb = menu->m_cb;
		void *userdata = menu->m_userdata;
		int32_t choice = menu->m_choice;
		delete menu;
		if (cb)
		{
			cb(choice, userdata);
		}
	}, this);
}

AboutMenu::AboutMenu(ui::Widget *parentRoot) : m_parentRoot(parentRoot)
{
	Plugin::PageOpenParam openParam;
	openParam.graphics_flag = 0x80;
	openParam.overwrite_draw_priority = 8;
	m_root = g_appPlugin->PageOpen(page_about, openParam);
	m_parentRoot->SetActivate(false);

	theme::ApplyBackground(m_root->FindChild(plane_base_bg_page_about));
	SetString(m_root->FindChild(text_top), msg_set_about);
	SetString(m_root->FindChild(text_about_page_about), msg_about_text);

	ui::Widget *back = m_root->FindChild(button_back_page_about);
	back->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE);
	back->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		AboutMenu *menu = reinterpret_cast<AboutMenu *>(userdata);
		menu->m_root->SetActivate(false);
		DeferDelete(menu);
	}, this);
}

AboutMenu::~AboutMenu()
{
	Plugin::PageCloseParam closeParam;
	closeParam.fade = true;
	closeParam.fade_time_ms = 200.0f;
	g_appPlugin->PageClose(page_about, closeParam);
	m_parentRoot->SetActivate(true);
}
