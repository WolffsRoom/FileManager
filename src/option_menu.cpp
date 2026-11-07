// From NetStream (GrapheneCt).
#include <sdk_compat.h>
#include <kernel.h>
#include <paf.h>

#include "common.h"
#include "event.h"
#include "option_menu.h"
#include <new>


// Not in the XML on purpose: PageOption_Create builds this page at runtime and fails
// when a page with the same name already exists in the resource.
#define page_option_menu (0xd098335e)

using namespace paf;

const float k_minBtLen = 202.0f;

OptionMenu *OptionMenu::Create(Plugin *plugin, ui::Widget *parent, paf::vector<Button> *buttons,
	uint32_t pageHash, int32_t tag)
{
	if (!plugin || !parent || !buttons || buttons->empty()) return NULL;
	// Do not call a C++ constructor with a null this pointer (seen in the Vita dumps).
	void *memory = ::operator new(sizeof(OptionMenu), std::nothrow);
	if (!memory) return NULL;
	OptionMenu *menu = new (memory) OptionMenu(plugin, parent, buttons, pageHash, tag);
	if (!menu->m_rootScene || !menu->m_optPlaneRoot)
	{
		delete menu;
		return NULL;
	}
	return menu;
}

void OptionMenu::OnButton(ui::Widget *wdg)
{
	if (m_closing || !wdg) return;
	m_closing = true;
	m_selectedHash = wdg->GetName().GetIDHash();
	if (m_rootScene) m_rootScene->SetActivate(false);
	// Destruction inside CB_BTN_DECIDE invalidates the widget tree while PAF is
	// still dispatching the event. Defer both destruction and the selection event.
	DeferCall([](void *userdata)
	{
		OptionMenu *menu = static_cast<OptionMenu *>(userdata);
		const int32_t tag = menu->m_tag;
		const uint32_t hash = menu->m_selectedHash;
		Plugin *plugin = menu->m_plugin;
		delete menu;
		if (hash == button_option_menu_close)
			event::BroadcastGlobalEvent(plugin, OptionMenuEvent, OptionMenuEvent_Close, 0, tag);
		else
			event::BroadcastGlobalEvent(plugin, OptionMenuEvent, OptionMenuEvent_Button, hash - OPTION_MENU_BUTTON_MAGIC, tag);
	}, this);
}

OptionMenu::OptionMenu(Plugin *workPlugin, ui::Widget *root, paf::vector<Button> *buttons, uint32_t pageHash, int32_t tag) :
	m_closing(false), m_selectedHash(0), m_plugin(workPlugin), m_rootScene(NULL),
	m_parentRoot(root), m_optPlaneRoot(NULL), m_buttonCount(buttons ? buttons->size() : 0),
	m_pageHash(pageHash), m_tag(tag)
{
	Plugin::TemplateOpenParam tmpParam;
	ui::Widget *closeButton = NULL;
	ui::Widget *button = NULL;


	Plugin::PageOpenParam openParam;
	openParam.option = Plugin::PageOption_Create;
	openParam.overwrite_draw_priority = 8;
	openParam.graphics_flag = 0x80;
	m_rootScene = m_plugin->PageOpen(m_pageHash, openParam);
	if (!m_rootScene)
	{
		SCE_DBG_LOG_ERROR("[OPTION_MENU] Attempt to create option menu twice!\n");
		return;
	}

	m_parentRoot->SetActivate(false);

	m_plugin->TemplateOpen(m_rootScene, template_option_menu_base, tmpParam);

	closeButton = m_rootScene->FindChild(button_option_menu_close);
	if (!closeButton) return;
	closeButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		if (!userdata || !self) return; // Vita coredumps: PAF dispatched with userdata == NULL
		reinterpret_cast<OptionMenu *>(userdata)->OnButton(static_cast<ui::Widget *>(self));
	}, this);

	closeButton->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE | inputdevice::pad::Data::PAD_MENU);

	m_optPlaneRoot = closeButton->FindChild(plane_option_menu_base);
	if (!m_optPlaneRoot) return;
	ui::Widget *optRoot = m_optPlaneRoot->FindChild(box_option_menu);
	if (!optRoot) { m_optPlaneRoot = NULL; return; }

	uint32_t maxLen = 0;
	uint32_t curLen = 0;
	uint32_t maxLenIdx = 0;
	uint32_t idhash = 0;

	for (int i = 0; i < m_buttonCount; i++)
	{
		Button btItem = buttons->at(i);

		if (i == 0)
		{
			if (m_buttonCount == 1)
			{
				idhash = template_option_menu_button_single;
			}
			else
			{
				idhash = template_option_menu_button_top;
			}
		}
		else if (i == m_buttonCount - 1)
		{
			idhash = template_option_menu_button_bottom;
		}
		else
		{
			idhash = template_option_menu_button_middle;
		}

		const int32_t before = optRoot->GetChildrenNum();
		m_plugin->TemplateOpen(optRoot, idhash, tmpParam);
		if (optRoot->GetChildrenNum() <= before)
		{
			m_optPlaneRoot = NULL;
			return; // allocation failed; Create() will clean up the partial page
		}
		button = optRoot->GetChild(optRoot->GetChildrenNum() - 1);
		if (!button) { m_optPlaneRoot = NULL; return; }
		button->SetName(OPTION_MENU_BUTTON_MAGIC + i);
		button->SetString(btItem.label);
		// Long translated labels stay centered inside the native button instead
		// of wrapping or disappearing beyond its bounds.
		graph::TextObj *labelObj = static_cast<graph::TextObj *>(button->GetDrawObj(ui::Button::OBJ_LABEL));
		if (labelObj && !btItem.label.empty())
		{
			float point = btItem.label.length() > 34 ? 18.0f : (btItem.label.length() > 24 ? 22.0f : 28.0f);
			labelObj->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, btItem.label.length(), point);
		}
		button->AddEventCallback(ui::Button::CB_BTN_DECIDE,
		[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
		{
			if (!userdata || !self) return; // Vita coredumps: PAF dispatched with userdata == NULL
		reinterpret_cast<OptionMenu *>(userdata)->OnButton(static_cast<ui::Widget *>(self));
		}, this);

		curLen = btItem.label.length();
		if (curLen > maxLen)
		{
			maxLen = curLen;
			maxLenIdx = i;
		}
	}

	ui::Widget *ruler = m_optPlaneRoot->FindChild(text_option_menu_ruler);
	if (!ruler) { m_optPlaneRoot = NULL; return; }
	ruler->SetString(buttons->at(maxLenIdx).label);

	// Size synchronously. The former CB_STATE_READY callback could run after the
	// menu had already closed, which caused the addcont navigation crash.
	float len = k_minBtLen + (maxLen > 18 ? (maxLen - 18) * 10.0f : 0.0f);
	if (len > 620.0f) len = 620.0f;
	math::v4 btSz(len, 60.0f);
	math::v4 plSz(len + 12.0f, 12.0f + 60.0f * m_buttonCount);
	math::v4 plPos(264.0f + ((202.0f - plSz.extract_x()) / 2.0f), 43.0f);
	for (int i = 0; i < m_buttonCount; i++)
	{
		button = optRoot->FindChild(OPTION_MENU_BUTTON_MAGIC + i);
		if (!button) { m_optPlaneRoot = NULL; return; }
		button->SetAdjust(ui::Widget::ADJUST_NONE, ui::Widget::ADJUST_NONE, ui::Widget::ADJUST_NONE);
		button->SetSize(btSz);
	}
	m_optPlaneRoot->SetSize(plSz);
	m_optPlaneRoot->SetPos(plPos);
	m_optPlaneRoot->Show(common::transition::Type_Popup4, 0.0f);
}

OptionMenu::~OptionMenu()
{
	Plugin::PageCloseParam closeParam;

	if (m_rootScene)
	{
		if (m_optPlaneRoot)
			m_optPlaneRoot->Hide(common::transition::Type_Reset, 0.0f);
		// A pending one-second fade keeps the page alive and causes duplicate
		// PageOpen and allocation crashes when the next menu is opened quickly.
		closeParam.fade = false;
		closeParam.fade_time_ms = 0.0f;
		m_plugin->PageClose(m_pageHash, closeParam);
	}
	if (m_parentRoot) m_parentRoot->SetActivate(true);
}
