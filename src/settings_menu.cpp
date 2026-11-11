#include <sdk_compat.h>
#include <paf.h>
#include <string.h>
#include <psp2/power.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>

#include "dialog.h"

#include "common.h"
#include "settings.h"
#include "settings_menu.h"
#include "core/fs.h"
#include "notice_menu.h"

using namespace paf;

namespace {
	const uint32_t k_switchOffOn[] = { tex_switch_off_on_00, tex_switch_off_on_01, tex_switch_off_on_02, tex_switch_off_on_03, tex_switch_off_on_04, tex_switch_off_on_05, tex_switch_off_on_06, tex_switch_off_on_07, tex_switch_off_on_08, tex_switch_off_on_09, tex_switch_off_on_10, tex_switch_off_on_11, tex_switch_off_on_12, tex_switch_off_on_13, tex_switch_off_on_14, tex_switch_off_on_15, tex_switch_off_on_16 };
	const uint32_t k_switchOnOff[] = { tex_switch_on_off_00, tex_switch_on_off_01, tex_switch_on_off_02, tex_switch_on_off_03, tex_switch_on_off_04, tex_switch_on_off_05, tex_switch_on_off_06, tex_switch_on_off_07, tex_switch_on_off_08, tex_switch_on_off_09, tex_switch_on_off_10, tex_switch_on_off_11, tex_switch_on_off_12, tex_switch_on_off_13, tex_switch_on_off_14, tex_switch_on_off_15, tex_switch_on_off_16 };

	void SwitchAnimationTask(void *userdata)
	{
		static_cast<SettingsMenu *>(userdata)->AdvanceSwitchAnimation();
	}

	// Main level rows
	enum MainRow { Main_Themes, Main_FontSize, Main_PreviewOpacity, Main_ShowDetails, Main_ShowVpkIcons, Main_Language, Main_Select, Main_AutoDecrypt, Main_Navigation, Main_Shortcuts, Main_Commands, Main_About, Main_Count };

	const uint32_t k_mainLabels[Main_Count] = { msg_set_themes, msg_set_font_size, msg_set_preview_opacity, msg_set_show_details, msg_set_vpk_icons, msg_set_language, msg_set_select, msg_set_auto_decrypt, msg_set_navigation, msg_set_shortcuts, msg_set_commands, msg_set_about };
	const uint32_t k_mainIcons[Main_Count] = { tex_new_theme, tex_ui_text, tex_ui_image, tex_ui_text, tex_ui_vpk, tex_ui_text, tex_ui_ftp, tex_ui_encrypted, tex_ui_anim_options, tex_ui_bookmark, tex_new_gears, tex_about };

	const uint32_t k_languages[] = { msg_lang_system, msg_lang_ptbr, msg_lang_en, msg_lang_es, msg_lang_fr, msg_lang_de };
	const uint32_t k_selectFunctions[] = { msg_ftp_server, msg_qr_code, msg_usb };
	const uint32_t k_usbDevices[] = { msg_usb_memory_card, msg_usb_game_card, msg_usb_sd2vita, msg_usb_psvsd };
	const uint32_t k_fontSizes[] = { msg_font_small, msg_font_normal, msg_font_large };
	const uint32_t k_enabled[] = { msg_disabled, msg_enabled };
	const uint32_t k_navigation[] = { msg_navigation_simple, msg_navigation_native };
	const uint32_t k_themeIcons[] = { tex_theme_graphite, tex_theme_midnight, tex_theme_graphite,
		tex_theme_ocean, tex_theme_purple, tex_theme_emerald, tex_theme_wine, tex_theme_pink,
		tex_ui_image, tex_theme_sony };

	// Button, then what it does
	const uint32_t k_commands[][3] = {
		{ msg_btn_cross, msg_cmd_open, tex_control_cross },
		{ msg_btn_circle, msg_cmd_back, tex_control_circle },
		{ msg_btn_triangle, msg_cmd_settings, tex_control_triangle },
		{ msg_btn_square, msg_cmd_mark, tex_control_square },
		{ msg_btn_select, msg_cmd_select, tex_control_select },
		{ msg_btn_start, msg_cmd_options, tex_button_start },
		{ msg_btn_l, msg_cmd_first, tex_control_l },
		{ msg_btn_r, msg_cmd_last, tex_control_r },
		{ msg_btn_left, msg_cmd_parent, tex_control_left },
		{ msg_btn_right, msg_cmd_enter, tex_control_right },
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
	m_levelQueued(false), m_queuedLevel(Level_Main), m_pendingPower(-1),
	m_switchAnimating(false), m_switchOldRow(-1), m_switchNewRow(-1), m_switchFrame(0), m_switchTick(0)
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
	m_opacitySlider = static_cast<ui::ProgressBarTouch *>(m_root->FindChild(progressbar_preview_opacity_page_settings));
	m_opacityLabel = static_cast<ui::Text *>(m_root->FindChild(text_preview_opacity_page_settings));
	m_opacitySlider->SetMinValue(25);
	m_opacitySlider->SetMaxValue(90);
	m_opacitySlider->AddEventCallback(ui::ProgressBarTouch::CB_PROGRESSBAR_CHANGED,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata)
	{
		reinterpret_cast<SettingsMenu *>(userdata)->OnOpacityChanged();
	}, this);
	m_opacitySlider->Hide(common::transition::Type_Reset);
	m_opacityLabel->Hide(common::transition::Type_Reset);
	m_list = static_cast<ui::ListView *>(m_root->FindChild(list_view_page_settings));
	m_list->SetItemFactory(new ListViewCb(this));
	m_list->InsertSegment(0, 1);
	math::v4 cellSize(960.0f, 80.0f);
	m_list->SetCellSizeDefault(0, cellSize);
	m_list->SetSegmentLayoutType(0, ui::ListView::LAYOUT_TYPE_LIST);

	ShowLevel(openUsbDevice ? Level_UsbDevice : Level_Main);
	common::MainThreadCallList::Register(SwitchAnimationTask, this);
}

SettingsMenu::~SettingsMenu()
{
	common::MainThreadCallList::Unregister(SwitchAnimationTask, this);
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
	case Level_PreviewOpacity: return 0;
	case Level_ShowDetails: return 2;
	case Level_ShowVpkIcons: return 2;
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
	// Rebuilding the page invalidates the list widgets used by an animation.
	m_switchAnimating = false;
	m_switchOldRow = -1;
	m_switchNewRow = -1;
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
	if (level == Level_PreviewOpacity)
	{
		m_list->Hide(common::transition::Type_Reset);
		m_opacitySlider->SetValue(settings::Get().previewOpacity, false);
		m_opacitySlider->Show(common::transition::Type_Reset);
		m_opacityLabel->Show(common::transition::Type_Reset);
		UpdateOpacityLabel();
	}
	else
	{
		m_list->Show(common::transition::Type_Reset);
		m_opacitySlider->Hide(common::transition::Type_Reset);
		m_opacityLabel->Hide(common::transition::Type_Reset);
	}

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

	static const uint32_t k_titles[] = { msg_opt_settings, msg_set_themes, msg_set_font_size, msg_set_preview_opacity, msg_set_show_details, msg_set_vpk_icons, msg_set_language, msg_set_select, msg_usb_device, msg_set_auto_decrypt, msg_set_navigation, msg_set_shortcuts, msg_set_commands };
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
	case Level_Themes: return row == (int32_t)theme::CustomIndex() ? c.customBackground != 0 : c.theme == row;
	case Level_FontSize: return c.fontSize == row;
	case Level_ShowDetails: return c.showDetails == row;
	case Level_ShowVpkIcons: return c.showVpkIcons == row;
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
	return m_level != Level_Main && m_level != Level_Commands && m_level != Level_PreviewOpacity;
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
	// The switch sits at x=401 with a 72 px hit area. Settings rows extend the
	// touch target over it while browser rows retain their original width.
	button->SetSize(920.0f, 80.0f, 0.0f);

	paf::wstring label, info;
	uint32_t icon = tex_new_settings;
	settings::Config &c = settings::Get();

	switch (m_level)
	{
	case Level_PreviewOpacity:
		break;
	case Level_Main:
		label = Str(k_mainLabels[row]);
		icon = k_mainIcons[row];
		if (row == Main_FontSize) icon = tex_ui_font_size;
		else if (row == Main_PreviewOpacity) icon = tex_ui_image;
		else if (row == Main_ShowDetails) icon = tex_ui_text;
		else if (row == Main_ShowVpkIcons) icon = tex_ui_vpk;
		else if (row == Main_Language) icon = tex_ui_language;
		else if (row == Main_Select) icon = c.selectFunction == 2 ? tex_ui_usb :
			(c.selectFunction == 1 ? tex_ui_qr : tex_ui_ftp);
		else if (row == Main_AutoDecrypt) icon = c.autoDecrypt ? tex_ui_unlock : tex_ui_locked;
		else if (row == Main_Navigation) icon = tex_ui_anim_options;
		else if (row == Main_Shortcuts) icon = tex_ui_favorite_folder;
		if (row == Main_Themes) info = Str(theme::At(c.theme).nameHash);
		else if (row == Main_FontSize) info = Str(k_fontSizes[c.fontSize]);
		else if (row == Main_PreviewOpacity) info = Wide(common::FormatString("%d%%", c.previewOpacity).c_str());
		else if (row == Main_ShowDetails) info = Str(k_enabled[c.showDetails]);
		else if (row == Main_ShowVpkIcons) info = Str(k_enabled[c.showVpkIcons]);
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
		icon = k_themeIcons[row];
		break;
	case Level_FontSize:
		label = Str(k_fontSizes[row]);
		icon = tex_ui_font_size;
		break;
	case Level_ShowDetails:
		label = Str(k_enabled[row]);
		icon = tex_ui_text;
		break;
	case Level_ShowVpkIcons:
		label = Str(k_enabled[row]);
		icon = tex_ui_vpk;
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

	button->SetString(L"");
	button->FindChild(text_list_item_name)->SetString(label);
	button->FindChild(text_list_item_info)->SetString(info);

	ui::Widget *check = item->FindChild(plane_list_item_check);
	ui::Widget *themePreview = button->FindChild(plane_theme_preview);
	if (themePreview) themePreview->Hide(common::transition::Type_Reset);
	if (check && ShowsToggle())
	{
		check->SetTexture(g_appPlugin->GetTexture(IsSelected(row) ? tex_switch_off_on_16 : tex_switch_off_on_00), 0, 0);
		check->Show(common::transition::Type_Reset);
	}
	else if (check) check->Hide(common::transition::Type_Reset);

	intrusive_ptr<graph::Surface> tex = g_appPlugin->GetTexture(icon);
	if (tex.get()) button->SetTexture(tex);
	// Settings list icons are uniformly 10% smaller than the shared file-row
	// default. Power actions are separate widgets and keep their own size.
	math::v3 iconSize;
	float iconValues[4] = {
		m_level == Level_Commands ? 64.0f : 91.8f,
		m_level == Level_Commands ? 44.0f : 63.0f, 0.0f, 0.0f
	};
	memcpy(&iconSize, iconValues, sizeof(iconSize));
	static_cast<ui::ImageButton *>(button)->SetTextureSize(iconSize);

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
			if (!(m_switchAnimating && (row == m_switchOldRow || row == m_switchNewRow)))
				check->SetTexture(g_appPlugin->GetTexture(IsSelected(row) ? tex_switch_off_on_16 : tex_switch_off_on_00), 0, 0);
			check->Show(common::transition::Type_Reset);
		}
		else
		{
			check->Hide(common::transition::Type_Reset);
		}
	}
}

void SettingsMenu::StartSwitchAnimation(int32_t oldRow, int32_t newRow)
{
	if (m_switchAnimating) return;
	ui::ListItem *oldItem = oldRow >= 0 ? m_list->GetListItem(0, oldRow) : NULL;
	ui::ListItem *newItem = newRow >= 0 ? m_list->GetListItem(0, newRow) : NULL;
	ui::Widget *oldCheck = oldItem ? oldItem->FindChild(plane_list_item_check) : NULL;
	ui::Widget *newCheck = newItem ? newItem->FindChild(plane_list_item_check) : NULL;
	if (!oldCheck && !newCheck) return;
	m_switchAnimating = true;
	m_switchOldRow = oldRow;
	m_switchNewRow = newRow;
	m_switchFrame = 0;
	m_switchTick = sceKernelGetProcessTimeWide();
	if (oldCheck) oldCheck->SetTexture(g_appPlugin->GetTexture(k_switchOnOff[0]), 0, 0);
	if (newCheck) newCheck->SetTexture(g_appPlugin->GetTexture(k_switchOffOn[0]), 0, 0);
}

void SettingsMenu::AdvanceSwitchAnimation()
{
	if (!m_switchAnimating) return;
	const uint64_t now = sceKernelGetProcessTimeWide();
	if (now - m_switchTick < 25000ULL) return;
	m_switchTick = now;
	if (++m_switchFrame > 16) m_switchFrame = 16;
	ui::ListItem *oldItem = m_switchOldRow >= 0 ? m_list->GetListItem(0, m_switchOldRow) : NULL;
	ui::ListItem *newItem = m_switchNewRow >= 0 ? m_list->GetListItem(0, m_switchNewRow) : NULL;
	ui::Widget *oldCheck = oldItem ? oldItem->FindChild(plane_list_item_check) : NULL;
	ui::Widget *newCheck = newItem ? newItem->FindChild(plane_list_item_check) : NULL;
	if (oldCheck) oldCheck->SetTexture(g_appPlugin->GetTexture(k_switchOnOff[m_switchFrame]), 0, 0);
	if (newCheck) newCheck->SetTexture(g_appPlugin->GetTexture(k_switchOffOn[m_switchFrame]), 0, 0);
	if (m_switchFrame >= 16)
	{
		m_switchAnimating = false;
		m_switchOldRow = -1;
		m_switchNewRow = -1;
		UpdateChecks();
	}
}

void SettingsMenu::UpdateFontPreview()
{
	if (!m_fontPreview) return;
	const float scale = settings::FileFontSize() / 28.0f;
	m_fontPreview->SetScale(scale, scale, 1.0f);
}

void SettingsMenu::UpdateOpacityLabel()
{
	if (!m_opacityLabel) return;
	paf::string format;
	common::Utf16ToUtf8(paf::wstring(g_appPlugin->GetString(msg_preview_opacity_hint)), &format);
	paf::string value = common::FormatString(format.c_str(), settings::Get().previewOpacity);
	m_opacityLabel->SetString(Wide(value.c_str()));
}

void SettingsMenu::OnOpacityChanged()
{
	int32_t value = (int32_t)(m_opacitySlider->GetValue() + 0.5f);
	if (value < 25) value = 25;
	if (value > 90) value = 90;
	settings::Get().previewOpacity = value;
	settings::Save();
	UpdateOpacityLabel();
}

void SettingsMenu::OnRow(int32_t row)
{
	settings::Config &c = settings::Get();
	int32_t oldRow = -1;
	if (ShowsToggle())
	{
		if (m_switchAnimating) return;
		const bool customToggle = m_level == Level_Themes && row == (int32_t)theme::CustomIndex();
		if (IsSelected(row) && !customToggle) return;
		for (int32_t i = 0; i < m_rows; ++i)
		{
			if (IsSelected(i)) { oldRow = i; break; }
		}
	}
	switch (m_level)
	{
	case Level_PreviewOpacity:
		return;
	case Level_Main:
		if (row == Main_Themes) QueueLevel(Level_Themes);
		else if (row == Main_FontSize) QueueLevel(Level_FontSize);
		else if (row == Main_PreviewOpacity) QueueLevel(Level_PreviewOpacity);
		else if (row == Main_ShowDetails) QueueLevel(Level_ShowDetails);
		else if (row == Main_ShowVpkIcons) QueueLevel(Level_ShowVpkIcons);
		else if (row == Main_Language) QueueLevel(Level_Language);
		else if (row == Main_Select) QueueLevel(Level_Select);
		else if (row == Main_AutoDecrypt) QueueLevel(Level_AutoDecrypt);
		else if (row == Main_Navigation) QueueLevel(Level_Navigation);
		else if (row == Main_Shortcuts) QueueLevel(Level_Shortcuts);
		else if (row == Main_Commands) QueueLevel(Level_Commands);
		else new AboutMenu(m_root);
		return;
	case Level_Themes:
		if (row == (int32_t)theme::CustomIndex()) c.customBackground = !c.customBackground;
		else c.theme = row;
		theme::ApplyBackground(m_root->FindChild(plane_base_bg_page_settings));
		break;
	case Level_FontSize:
		c.fontSize = row;
		UpdateFontPreview();
		break;
	case Level_ShowDetails:
		c.showDetails = row;
		break;
	case Level_ShowVpkIcons:
		c.showVpkIcons = row;
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
	if (ShowsToggle() && !(m_level == Level_Themes && row == (int32_t)theme::CustomIndex()))
		StartSwitchAnimation(oldRow, row);
}
