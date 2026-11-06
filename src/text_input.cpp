#include <sdk_compat.h>
#include <paf.h>

#include "common.h"
#include "text_input.h"

using namespace paf;

TextInput::TextInput(ui::Widget *parentRoot, const wchar_t *title, const paf::wstring &initial, Callback cb, void *userdata) :
	m_parentRoot(parentRoot), m_cb(cb), m_userdata(userdata)
{
	Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = 8;
	openParam.graphics_flag = 0x80;
	m_root = g_appPlugin->PageOpen(page_text_input, openParam);

	m_parentRoot->SetActivate(false);

	paf::wstring titleStr = title;
	m_root->FindChild(text_title_page_text_input)->SetString(titleStr);

	m_box = static_cast<ui::TextBox *>(m_root->FindChild(text_box_page_text_input));
	paf::wstring value = initial;
	m_box->SetString(value);
	m_box->AddEventCallback(ui::TextBox::CB_TEXT_BOX_ENTER_PRESSED,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<TextInput *>(userdata)->Finish(true);
	}, this);

	ui::Widget *cancel = m_root->FindChild(button_cancel_page_text_input);
	cancel->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE);
	cancel->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<TextInput *>(userdata)->Finish(false);
	}, this);
}

TextInput::~TextInput()
{
	Plugin::PageCloseParam closeParam;
	closeParam.fade = true;
	closeParam.fade_time_ms = 200.0f;
	g_appPlugin->PageClose(page_text_input, closeParam);
	m_parentRoot->SetActivate(true);
}

void TextInput::Finish(bool accepted)
{
	paf::wstring result;
	if (accepted)
	{
		m_box->GetString(result);
	}

	m_result = result;
	m_root->SetActivate(false);
	DeferCall([](void *p)
	{
		TextInput *input = static_cast<TextInput *>(p);
		Callback cb = input->m_cb;
		void *userdata = input->m_userdata;
		paf::wstring text = input->m_result;
		delete input;
		cb(text, userdata);
	}, this);
}
