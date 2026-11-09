#ifndef _FM_SETTINGS_MENU_H_
#define _FM_SETTINGS_MENU_H_

#include <sdk_compat.h>
#include <paf.h>

/**
 * Settings page with levels: the main list (Themes, Language, Select function,
 * Shortcuts, Commands) and one list per entry. Circle goes up a level, then closes.
 * Changes apply and save at once.
 */
class SettingsMenu
{
public:

	typedef void (*ChangeCallback)(void *userdata);

	enum Level
	{
		Level_Main,
		Level_Themes,
		Level_FontSize,
		Level_Language,
		Level_Select,
		Level_UsbDevice,
		Level_AutoDecrypt,
		Level_Navigation,
		Level_Shortcuts,
		Level_Commands
	};

	class ListViewCb : public paf::ui::listview::ItemFactory
	{
	public:

		ListViewCb(SettingsMenu *parent) : m_parent(parent) {}
		~ListViewCb() {}

		paf::ui::ListItem *Create(CreateParam &param)
		{
			return m_parent->CreateItem(param);
		}

		void Start(StartParam &param)
		{
			param.list_item->Show(paf::common::transition::Type_Popup1);
		}
		void Stop(StopParam &param)
		{
			param.list_item->Hide(paf::common::transition::Type_Reset);
		}

	private:

		SettingsMenu *m_parent;
	};

	SettingsMenu(paf::ui::Widget *parentRoot, ChangeCallback cb, void *userdata, bool openUsbDevice = false);
	~SettingsMenu();

	paf::ui::ListItem *CreateItem(paf::ui::listview::ItemFactory::CreateParam &param);
	void AdvanceSwitchAnimation();

	paf::ui::Scene *m_root;

private:

	void OnBack();
	void OnRow(int32_t row);
	void ShowLevel(Level level);
	void QueueLevel(Level level);
	void OnPowerRequested(int32_t action);
	void OnPowerDialog(int32_t buttonCode);
	int32_t RowCount(Level level);
	bool IsSelected(int32_t row);
	bool ShowsToggle() const;
	void UpdateChecks();
	void UpdateFontPreview();
	void StartSwitchAnimation(int32_t oldRow, int32_t newRow);

	paf::ui::Widget *m_parentRoot;
	paf::ui::ListView *m_list;
	paf::ui::Widget *m_emptyShortcuts;
	paf::ui::Widget *m_powerBar;
	paf::ui::Widget *m_fontPreview;
	bool m_levelQueued;
	Level m_queuedLevel;
	int32_t m_pendingPower;
	bool m_switchAnimating;
	int32_t m_switchOldRow;
	int32_t m_switchNewRow;
	int32_t m_switchFrame;
	uint64_t m_switchTick;
	ChangeCallback m_cb;
	void *m_userdata;
	Level m_level;
	int32_t m_rows;
	paf::vector<paf::string> m_shortcuts;
	paf::vector<int32_t> m_shortcutKept;
};

#endif
