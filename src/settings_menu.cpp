#include <sdk_compat.h>
#include <paf.h>
#include <string.h>
#include <psp2/power.h>
#include <psp2/io/stat.h>

#include "dialog.h"

#include "common.h"
#include "settings.h"
#include "settings_menu.h"
#include "core/fs.h"
#include "notice_menu.h"

using namespace paf;

namespace {

	// Main level rows
	enum MainRow { Main_Themes, Main_FontSize, Main_Language, Main_Select, Main_AutoDecrypt, Main_Navigation, Main_Shortcuts, Main_Commands, Main_About, Main_Count };

	const uint32_t k_mainLabels[Main_Count] = { msg_set_themes, msg_set_font_size, msg_set_language, msg_set_select, msg_set_auto_decrypt, msg_set_navigation, msg_set_shortcuts, msg_set_commands, msg_set_about };
	const uint32_t k_mainIcons[Main_Count] = { tex_new_theme, tex_ui_text, tex_ui_text, tex_ui_ftp, tex_ui_encrypted, tex_ui_anim_options, tex_ui_bookmark, tex_new_gears, tex_about };

	const uint32_t k_languages[] = { msg_lang_system, msg_lang_ptbr, msg_lang_en, msg_lang_es, msg_lang_fr, msg_lang_de };
	const uint32_t k_selectFunctions[] = { msg_ftp_server, msg_qr_code, msg_usb };
	const uint32_t k_usbDevices[] = { msg_usb_memory_card, msg_usb_game_card, msg_usb_sd2vita, msg_usb_psvsd };
	const uint32_t k_fontSizes[] = { msg_font_small, msg_font_normal, msg_font_large };
	const uint32_t k_enabled[] = { msg_disabled, msg_enabled };
	const uint32_t k_navigation[] = { msg_navigation_simple, msg_navigation_native };

	// Button, then what it does
	const uint32_t k_commands[][3] = {
		{ msg_btn_cross, msg_cmd_open, tex_button_cross },
		{ msg_btn_circle, msg_cmd_back, tex_button_circle },
		{ msg_btn_triangle, msg_cmd_settings, tex_button_triangle },
		{ msg_btn_square, msg_cmd_mark, tex_button_square },
		{ msg_btn_select, msg_cmd_select, tex_button_select },
		{ msg_btn_start, msg_cmd_options, tex_button_start },
		{ msg_btn_l, msg_cmd_first, tex_button_l },
		{ msg_btn_r, msg_cmd_last, tex_button_r },
		{ msg_btn_left, msg_cmd_parent, tex_button_left },
		{ msg_btn_right, msg_cmd_enter, tex_new_arrow },
		{ msg_btn_hold, msg_cmd_mark, tex_button_touch },
	};
	const int32_t k_commandCount = sizeof(k_commands) / sizeof(k_commands[0]);

	paf::wstring Str(uint32_t id)
	{
		// Through UTF-8 so the PAF builds the string itself.
		paf::string s;
		common::Utf16ToUtf8(paf::wstring(g_appPlugin->GetString(id)), &s);
		paf::wstring w;
		common::Utf8ToUtf16(s, &w);
		return w;
	}

	paf::wstring Wide(const char *s)
	{
		paf::wstring w;
		common::Utf8ToUtf16(s, &w);
		return w;
	}
}

SettingsMenu::SettingsMenu(ui::Widget *parentRoot, ChangeCallback cb, void *userdata, bool openUsbDevice) :
	m_parentRoot(parentRoot), m_cb(cb), m_userdata(userdata), m_level(Level_Main), m_rows(0),
	m_levelQueued(false), m_queuedLevel(Level_Main), m_pendingPower(-1)
{
	Plugin::PageOpenParam openParam;
	openParam.graphics_flag = 0x80;
	openParam.overwrite_draw_priority = 8;
	m_root = g_appPlugin->PageOpen(page_settings, openParam);

	m_parentRoot->SetActivate(false);

	theme::ApplyBackground(m_root->FindChild(plane_base_bg_page_settings));

	ui::Widget *back = m_root->FindChild(button_back_page_settings);
	back->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE);
	back->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<SettingsMenu *>(userdata)->OnBack();
	}, this);

	m_powerBar = m_root->FindChild(plane_power_actions_page_settings);
	static const uint32_t powerButtons[] = {
		button_power_off_page_settings, button_power_restart_page_settings,
		button_power_sleep_page_settings
	};
	static const uint32_t powerLabels[] = { msg_power_off, msg_power_restart, msg_power_sleep };
	for (int32_t i = 0; i < 3; ++i)
	{
		ui::Widget *button = m_root->FindChild(powerButtons[i]);
		button->SetString(Str(powerLabels[i]));
		button->AddEventCallback(ui::Button::CB_BTN_DECIDE,
		[](int32_t, ui::Handler *self, ui::Event *, void *user)
		{
			uint32_t id = static_cast<ui::Widget *>(self)->GetName().GetIDHash();
			int action = id == button_power_off_page_settings ? 0 :
				(id == button_power_restart_page_settings ? 1 : 2);
			reinterpret_cast<SettingsMenu *>(user)->OnPowerRequested(action);
		}, this);
	}
	m_root->AddEventCallback(dialog::DialogEvent,
	[](int32_t, ui::Handler *, ui::Event *e, void *user)
	{
		reinterpret_cast<SettingsMenu *>(user)->OnPowerDialog(e->GetValue(0));
	}, this);

	m_emptyShortcuts = m_root->FindChild(text_shortcuts_empty_page_settings);
	m_emptyShortcuts->SetString(Str(msg_shortcuts_empty));
	m_emptyShortcuts->Hide(common::transition::Type_Reset);
	m_fontPreview = m_root->FindChild(text_font_preview_page_settings);
	m_fontPreview->SetString(Str(msg_font_preview));
	m_fontPreview->Hide(common::transition::Type_Reset);
	m_list = static_cast<ui::ListView *>(m_root->FindChild(list_view_page_settings));
	m_list->SetItemFactory(new ListViewCb(this));
	m_list->InsertSegment(0, 1);
	math::v4 cellSize(960.0f, 80.0f);
	m_list->SetCellSizeDefault(0, cellSize);
	m_list->SetSegmentLayoutType(0, ui::ListView::LAYOUT_TYPE_LIST);

	ShowLevel(openUsbDevice ? Level_UsbDevice : Level_Main);
}

SettingsMenu::~SettingsMenu()
{
	m_list->SetName(static_cast<uint32_t>(0));
	Plugin::PageCloseParam closeParam;
	closeParam.fade = true;
	closeParam.fade_time_ms = 200.0f;
	g_appPlugin->PageClose(page_settings, closeParam);
	m_parentRoot->SetActivate(true);
	if (m_cb) m_cb(m_userdata);
}

void SettingsMenu::OnBack()
{
	if (m_level != Level_Main)
	{
		QueueLevel(Level_Main);
		return;
	}
	m_root->SetActivate(false);
	DeferDelete(this);
}

void SettingsMenu::QueueLevel(Level level)
{
	// PAF is still dispatching the selected ListView row callback. Changing
	// its cells synchronously can invalidate the current widget and crash.
	m_queuedLevel = level;
	if (m_levelQueued) return;
	m_levelQueued = true;
	DeferCall([](void *user)
	{
		SettingsMenu *menu = static_cast<SettingsMenu *>(user);
		menu->m_levelQueued = false;
		menu->ShowLevel(menu->m_queuedLevel);
	}, this);
}

void SettingsMenu::OnPowerRequested(int32_t action)
{
	if (m_pendingPower >= 0 || action < 0 || action > 2 || dialog::IsActive()) return;
	static const uint32_t k_prompts[] = {
		msg_power_confirm_off, msg_power_confirm_restart, msg_power_confirm_sleep
	};
	m_pendingPower = action;
	dialog::OpenYesNo(g_appPlugin, g_appPlugin->GetString(msg_opt_settings),
		g_appPlugin->GetString(k_prompts[action]));
}

void SettingsMenu::OnPowerDialog(int32_t buttonCode)
{
	if (m_pendingPower < 0) return;
	const int32_t action = m_pendingPower;
	m_pendingPower = -1;
	if (buttonCode != dialog::ButtonCode_Yes) return;
	// User-level ScePower functions are exactly those used by FMVita.
	// Standby requests shutdown; Suspend enters sleep mode.
	int32_t result = 0;
	if (action == 0) result = scePowerRequestStandby();
	else if (action == 1) result = scePowerRequestColdReset();
	else result = scePowerRequestSuspend();
	if (result < 0) dialog::OpenError(g_appPlugin, result);
}

int32_t SettingsMenu::RowCount(Level level)
{
	switch (level)
	{
	case Level_Main: return Main_Count;
	case Level_Themes: return theme::Count();
	case Level_FontSize: return 3;
	case Level_Language: return sizeof(k_languages) / sizeof(k_languages[0]);
	case Level_Select: return 3;
	case Level_UsbDevice: return 4;
	case Level_AutoDecrypt: return 2;
	case Level_Navigation: return 2;
	case Level_Shortcuts: return m_shortcuts.size();
	case Level_Commands: return k_commandCount;
	}
	return 0;
}

void SettingsMenu::ShowLevel(Level level)
{
	if (m_rows > 0)
	{
		m_list->DeleteCell(0, 0, m_rows);
	}
	m_level = level;
	if (level == Level_Main)
	{
		static const uint32_t ids[] = {
			button_power_off_page_settings, button_power_restart_page_settings, button_power_sleep_page_settings
		};
		static const uint32_t texts[] = { msg_power_off, msg_power_restart, msg_power_sleep };
		for (int i = 0; i < 3; ++i)
			m_root->FindChild(ids[i])->SetString(Str(texts[i]));
		m_powerBar->Show(common::transition::Type_Reset);
		// Leave a visible neutral gap between power actions and list rows.
		m_list->SetSize(960.0f, 304.0f, 0.0f);
		m_list->SetPos(0.0f, -254.0f, 0.0f);
	}
	else
	{
		m_powerBar->Hide(common::transition::Type_Reset);
		m_list->SetSize(960.0f, 448.0f, 0.0f);
		m_list->SetPos(0.0f, -272.0f, 0.0f);
	}
	if (level == Level_FontSize)
	{
		m_list->SetSize(960.0f, 240.0f, 0.0f);
		m_list->SetPos(0.0f, -148.0f, 0.0f);
		m_fontPreview->Show(common::transition::Type_Reset);
		UpdateFontPreview();
	}
	else m_fontPreview->Hide(common::transition::Type_Reset);

	if (level == Level_Shortcuts)
	{
		// A working copy: unticking removes at once, ticking again restores.
		m_shortcuts = shortcuts::List();
		m_shortcutKept.clear();
		for (uint32_t i = 0; i < m_shortcuts.size(); i++)
		{
			m_shortcutKept.push_back(1);
		}
	}

	static const uint32_t k_titles[] = { msg_opt_settings, msg_set_themes, msg_set_font_size, msg_set_language, msg_set_select, msg_usb_device, msg_set_auto_decrypt, msg_set_navigation, msg_set_shortcuts, msg_set_commands };
	m_root->FindChild(text_top)->SetString(Str(k_titles[level]));

	if (level == Level_Shortcuts && m_shortcuts.empty())
		m_emptyShortcuts->Show(common::transition::Type_Reset);
	else m_emptyShortcuts->Hide(common::transition::Type_Reset);

	m_rows = RowCount(level);
	if (m_rows > 0)
	{
		m_list->InsertCell(0, 0, m_rows);
	}
}

bool SettingsMenu::IsSelected(int32_t row)
{
	settings::Config &c = settings::Get();
	switch (m_level)
	{
	case Level_Themes: return c.theme == row;
	case Level_FontSize: return c.fontSize == row;
	case Level_Language: return c.language == row;
	case Level_Select: return c.selectFunction == row;
	case Level_UsbDevice: return c.usbDevice == row;
	case Level_AutoDecrypt: return c.autoDecrypt == row;
	case Level_Navigation: return c.navigation3D == row;
	case Level_Shortcuts: return row >= 0 && row < (int32_t)m_shortcuts.size() &&
		shortcuts::Contains(m_shortcuts.at(row));
	default: return false;
	}
}

bool SettingsMenu::ShowsToggle() const
{
	return m_level != Level_Main && m_level != Level_Commands;
}

ui::ListItem *SettingsMenu::CreateItem(ui::listview::ItemFactory::CreateParam &param)
{
	if (!param.list_view->GetName().GetIDHash() || param.cell_index >= m_rows)
	{
		return new ui::ListItem(param.parent, 0);
	}

	Plugin::TemplateOpenParam tmpParam;
	const int32_t row = param.cell_index;

	ui::Widget *targetRoot = param.parent;
	g_appPlugin->TemplateOpen(targetRoot, template_list_item_file, tmpParam);
	ui::Widget *item = targetRoot->GetChild(targetRoot->GetChildrenNum() - 1);

	ui::Widget *button = item->FindChild(image_button_list_item);
	button->SetName(row);

	paf::wstring label, info;
	uint32_t icon = tex_new_settings;
	settings::Config &c = settings::Get();

	switch (m_level)
	{
	case Level_Main:
		label = Str(k_mainLabels[row]);
		icon = k_mainIcons[row];
		if (row == Main_FontSize) icon = tex_ui_font_size;
		else if (row == Main_Language) icon = tex_ui_language;
		else if (row == Main_Select) icon = c.selectFunction == 2 ? tex_ui_usb :
			(c.selectFunction == 1 ? tex_ui_qr : tex_ui_ftp);
		else if (row == Main_AutoDecrypt) icon = c.autoDecrypt ? tex_ui_unlock : tex_ui_locked;
		else if (row == Main_Navigation) icon = tex_ui_anim_options;
		else if (row == Main_Shortcuts) icon = tex_ui_favorite_folder;
		if (row == Main_Themes) info = Str(theme::At(c.theme).nameHash);
		else if (row == Main_FontSize) info = Str(k_fontSizes[c.fontSize]);
		else if (row == Main_Language) info = Str(k_languages[c.language]);
		else if (row == Main_Select)
		{
			info = Str(k_selectFunctions[c.selectFunction]);
			if (c.selectFunction == 2 && c.usbDevice >= 0 && c.usbDevice < 4)
			{
				info += L" - ";
				info += Str(k_usbDevices[c.usbDevice]);
			}
		}
		else if (row == Main_AutoDecrypt) info = Str(k_enabled[c.autoDecrypt]);
		else if (row == Main_Navigation) info = Str(k_navigation[c.navigation3D]);
		else if (row == Main_Shortcuts) info = Wide(common::FormatString("%u", (uint32_t)shortcuts::List().size()).c_str());
		break;
	case Level_Themes:
		label = Str(theme::At(row).nameHash);
		icon = row == (int32_t)theme::CustomIndex() ? tex_ui_image : tex_new_theme;
		break;
	case Level_FontSize:
		label = Str(k_fontSizes[row]);
		icon = tex_ui_font_size;
		break;
	case Level_Language:
		label = Str(k_languages[row]);
		icon = tex_ui_language;
		break;
	case Level_Select:
		label = Str(k_selectFunctions[row]);
		icon = row == 2 ? tex_ui_usb : (row == 1 ? tex_ui_qr : tex_ui_ftp);
		break;
	case Level_UsbDevice:
		label = Str(k_usbDevices[row]);
		icon = tex_ui_usb;
		break;
	case Level_AutoDecrypt:
		label = Str(k_enabled[row]);
		icon = row ? tex_ui_unlock : tex_ui_locked;
		break;
	case Level_Navigation:
		label = Str(k_navigation[row]);
		icon = tex_ui_anim_options;
		break;
	case Level_Shortcuts:
		label = Wide(m_shortcuts.at(row).c_str());
		SceIoStat stat;
		icon = sceIoGetstat(m_shortcuts.at(row).c_str(), &stat) >= 0 &&
			!SCE_S_ISDIR(stat.st_mode) ? tex_ui_favorite_file : tex_ui_favorite_folder;
		break;
	case Level_Commands:
		label = Str(k_commands[row][1]);
		icon = k_commands[row][2];
		break;
	}

	button->SetString(label);
	button->FindChild(text_list_item_info)->SetString(info);

	ui::Widget *check = item->FindChild(plane_list_item_check);
	ui::Widget *themePreview = button->FindChild(plane_theme_preview);
	if (themePreview) themePreview->Hide(common::transition::Type_Reset);
	if (check && ShowsToggle())
	{
		check->SetTexture(g_appPlugin->GetTexture(IsSelected(row) ? tex_ui_toggle_on : tex_ui_toggle_off), 0, 0);
		check->Show(common::transition::Type_Reset);
	}
	else if (check) check->Hide(common::transition::Type_Reset);

	intrusive_ptr<graph::Surface> tex = g_appPlugin->GetTexture(icon);
	if (tex.get())
	{
		button->SetTexture(tex);
	}
	// Settings list icons are uniformly 10% smaller than the shared file-row
	// default. Power actions are separate widgets and keep their own size.
	math::v3 iconSize;
	float iconValues[4] = { 91.8f, 63.0f, 0.0f, 0.0f };
	memcpy(&iconSize, iconValues, sizeof(iconSize));
	static_cast<ui::ImageButton *>(button)->SetTextureSize(iconSize);
	if (m_level == Level_Themes)
	{
		const theme::Info &preview = theme::At(row);
		if (themePreview && row != (int32_t)theme::CustomIndex())
		{
			themePreview->SetTexture(g_appPlugin->GetTexture(tex_new_theme), 0, 0);
			themePreview->SetColor(preview.bg[0], preview.bg[1], preview.bg[2], 0.88f);
			themePreview->Show(common::transition::Type_Reset);
		}
	}

	button->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<SettingsMenu *>(userdata)->OnRow(static_cast<ui::Widget *>(self)->GetName().GetIDHash());
	}, this);

	return static_cast<ui::ListItem *>(item);
}

void SettingsMenu::UpdateChecks()
{
	for (int32_t row = 0; row < m_rows; row++)
	{
		ui::ListItem *item = m_list->GetListItem(0, row);
		if (!item)
		{
			continue;
		}
		ui::Widget *check = item->FindChild(plane_list_item_check);
		if (!check) continue;
		if (ShowsToggle())
		{
			check->SetTexture(g_appPlugin->GetTexture(IsSelected(row) ? tex_ui_toggle_on : tex_ui_toggle_off), 0, 0);
			check->Show(common::transition::Type_Reset);
		}
		else
		{
			check->Hide(common::transition::Type_Reset);
		}
	}
}

void SettingsMenu::UpdateFontPreview()
{
	if (!m_fontPreview) return;
	const float scale = settings::FileFontSize() / 28.0f;
	m_fontPreview->SetScale(scale, scale, 1.0f);
}

void SettingsMenu::OnRow(int32_t row)
{
	settings::Config &c = settings::Get();
	switch (m_level)
	{
	case Level_Main:
		if (row == Main_Themes) QueueLevel(Level_Themes);
		else if (row == Main_FontSize) QueueLevel(Level_FontSize);
		else if (row == Main_Language) QueueLevel(Level_Language);
		else if (row == Main_Select) QueueLevel(Level_Select);
		else if (row == Main_AutoDecrypt) QueueLevel(Level_AutoDecrypt);
		else if (row == Main_Navigation) QueueLevel(Level_Navigation);
		else if (row == Main_Shortcuts) QueueLevel(Level_Shortcuts);
		else if (row == Main_Commands) QueueLevel(Level_Commands);
		else new AboutMenu(m_root);
		return;
	case Level_Themes:
		c.theme = row;
		theme::ApplyBackground(m_root->FindChild(plane_base_bg_page_settings));
		break;
	case Level_FontSize:
		c.fontSize = row;
		UpdateFontPreview();
		break;
	case Level_Language:
		c.language = row;
		settings::ApplyLanguage();
		// Rows of this page take the new strings when rebuilt.
		QueueLevel(Level_Language);
		break;
	case Level_Select:
		c.selectFunction = row;
		if (row == 2) { settings::Save(); QueueLevel(Level_UsbDevice); return; }
		break;
	case Level_UsbDevice:
		c.usbDevice = row;
		break;
	case Level_AutoDecrypt:
		c.autoDecrypt = row;
		break;
	case Level_Navigation:
		c.navigation3D = row;
		break;
	case Level_Shortcuts:
		if (row >= 0 && row < (int32_t)m_shortcuts.size())
		{
			// A removed favorite must immediately disappear from this page.
			shortcuts::Remove(m_shortcuts.at(row));
			QueueLevel(Level_Shortcuts);
		}
		return;
	case Level_Commands:
		return;
	}
	settings::Save();
	UpdateChecks();
}
