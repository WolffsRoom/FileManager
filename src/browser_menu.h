#ifndef _FM_BROWSER_MENU_H_
#define _FM_BROWSER_MENU_H_

#include <sdk_compat.h>
#include <paf.h>

#include "core/fs.h"

/**
 * The main screen: devices first, then folders, each level on its own list view
 * with the system's slide transitions. Page stack adapted from NetStream's
 * GenericServerMenu (GrapheneCt).
 */
class BrowserMenu
{
public:

	enum Action
	{
		Action_Select,
		Action_ClearSelection,
		Action_Copy,
		Action_Move,
		Action_Paste,
		Action_Delete,
		Action_Rename,
		Action_NewFolder,
		Action_Info,
		Action_Settings,
		Action_More,
		Action_AddShortcut,
		Action_RemoveShortcut,
		Action_Ftp,
		Action_Qr
	};

	enum PendingDialog
	{
		Pending_None,
		Pending_ConfirmDelete,
		Pending_ConfirmInstall,
		Pending_Operation,
		Pending_Message,
		Pending_Ftp
	};

	class ListViewCb : public paf::ui::listview::ItemFactory
	{
	public:

		ListViewCb(BrowserMenu *parent) : m_parent(parent) {}
		~ListViewCb() {}

		paf::ui::ListItem *Create(CreateParam &param)
		{
			return m_parent->CreateListItem(param);
		}

		void Start(StartParam &param)
		{
			param.list_item->Show(paf::common::transition::Type_Popup1);
		}

	private:

		BrowserMenu *m_parent;
	};

	struct BrowserPage
	{
		paf::string path;           // empty = the device list
		fs::EntryList all;          // owns the entries
		fs::EntryList entries;      // shown: all, or the search matches
		paf::ui::ListView *list;
		uint32_t cellCount;
		int32_t focused;
		float width;                // current list width
		volatile bool loaded;
	};

	BrowserMenu();
	~BrowserMenu();

	paf::ui::ListItem *CreateListItem(paf::ui::listview::ItemFactory::CreateParam &param);

	/** Worker-thread side of opening or refreshing a page. */
	void LoadPage(BrowserPage *page, bool refresh);

	/** Worker-thread side of a file operation. */
	void RunOperation();

	/** Worker-thread side of the information dialog. */
	void RunInfo();

	void OnNameEntered(const paf::wstring &text);

	void OnFtpClosed(bool stopped);
	void DoActionPublic(Action action);
	void OnInstallAnswer(bool accepted);
	void OnSearch();
	void JumpTo(bool end);
	void OnSelectButton();

	/** The theme changed in the settings page. */
	void ApplySettings();


private:

	void PushPage(const paf::string &path);
	bool PopPage();
	void Refresh();
	BrowserPage *Top() { return m_pages.back(); }
	void UpdateChrome();
	void OpenPreview(int32_t index);
	void FillFileItem(BrowserPage *page, paf::ui::Widget *button, fs::Entry *entry);
	void FillRootItem(BrowserPage *page, paf::ui::Widget *button, fs::Entry *entry);
	paf::intrusive_ptr<paf::graph::Surface> Icon(uint32_t hash);

	void OnBack();
	void OnForward();
	void OpenOptionMenu(bool submenu);
	void StartFtp();
	void ReloadRoot();
	void OnOptions();
	void OnItemDecide(paf::ui::Widget *wdg);
	void OnItemFocus(paf::ui::Widget *wdg);
	void OnMarkToggle(int32_t index);
	void OnOption(int32_t index);
	void OnDialog(int32_t buttonCode);

	void DoAction(Action action);
	void SetClipboard(bool isMove);
	void CollectSelection(paf::vector<paf::string> *out);
	void StartOperation(int32_t type);
	void FinishOperation(int32_t result);

	enum OperationType
	{
		Op_Copy,
		Op_Move,
		Op_Delete,
		Op_Install
	};

	paf::ui::Scene *m_root;
	paf::ui::Widget *m_browserRoot;
	paf::ui::Widget *m_backButton;
	paf::ui::Widget *m_optionsButton;
	paf::ui::BusyIndicator *m_busy;
	paf::ui::Text *m_topText;
	paf::ui::Text *m_emptyText;
	paf::ui::Widget *m_background;
	paf::ui::TextBox *m_search;

	// Fetching an icon's texture once per list cell is a likely cause of slow lists.
	struct CachedIcon
	{
		uint32_t hash;
		paf::intrusive_ptr<paf::graph::Surface> tex;
	};
	CachedIcon m_icons[16];
	uint32_t m_iconCount;

	paf::vector<BrowserPage *> m_pages;
	paf::vector<Action> m_optionActions;

	paf::vector<paf::string> m_clipboard;
	bool m_clipboardIsMove;

	PendingDialog m_pending;
	int32_t m_opType;
	paf::vector<paf::string> m_opSources;
	paf::string m_opTargetDir;
	Action m_inputAction;
	paf::string m_inputTarget;
	paf::string m_installedTitle;
	paf::string m_scratch;
};

#endif
