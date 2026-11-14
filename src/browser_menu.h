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
		Action_Qr,
		Action_Sort,
		Action_SortDate,
		Action_SortName,
		Action_SortType,
		Action_Decrypt,
		Action_Undo,
		Action_Extract,
		Action_CompressZip,
		Action_ExportMedia,
		Action_InstallFolder,
		Action_RefreshLiveArea
	};

	enum PendingDialog
	{
		Pending_None,
		Pending_ConfirmDelete,
		Pending_ConfirmInstall,
		Pending_QrOpen,
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
			// Match the initial File Manager on every page. Stop() resets a
			// recycled cell before its next Popup1 so transforms never stack.
			param.list_item->Show(paf::common::transition::Type_Popup1);
		}

		void Stop(StopParam &param)
		{
			// ListView recycles rows. Reset the completed animation before the
			// same cell is started again so Popup1 never accumulates per scroll.
			param.list_item->Hide(paf::common::transition::Type_Reset);
		}

	private:

		BrowserMenu *m_parent;
	};

	struct BrowserPage
	{
		paf::string path;           // empty = the device list
		paf::string logicalPath;    // original path while a protected PFS is mounted
		bool ownsMount;
		bool ownsPsarc;
		fs::EntryList all;          // owns the entries
		fs::EntryList entries;      // shown: all, or the search matches
		paf::ui::ListView *list;
		uint32_t cellCount;
		int32_t focused;
		int32_t opened;             // row whose folder is open above this page
		float width;                // current list width
		volatile bool loaded;
	};

	BrowserMenu();
	~BrowserMenu();

	paf::ui::ListItem *CreateListItem(paf::ui::listview::ItemFactory::CreateParam &param);

	/** Worker-thread side of opening or refreshing a page. */
	void LoadPage(BrowserPage *page, bool refresh);
	/** Commit ListView changes on the real PAF UI frame; I/O is done by LoadJob. */
	void CommitLoad(BrowserPage *page, fs::EntryList *fresh, int32_t result);

	/** Worker-thread side of a file operation. */
	void RunOperation();
	void RunUndo();

	/** Worker-thread side of the information dialog. */
	void RunInfo();
	void RunQrDownload();

	void OnNameEntered(const paf::wstring &text);

	void OnFtpClosed(bool stopped);
	void OnQrResult(const paf::string &payload);
	const paf::string &InstalledTitle() const { return m_installedTitle; }
	void DoActionPublic(Action action);
	void OnInstallAnswer(bool accepted);
	void OnBackgroundInstallChoice(int32_t choice);
	void OnSearch();
	void FilterSearch(bool finishEditing);
	void PollSearch();
	void JumpTo(bool end);
	void OnSelectButton();
	void ApplySelectButton();
	void SelectAll();
	void ToggleSearch();
	void SetSearchVisible(bool visible);
	void CloseSearchForNavigation();
	void SetRowIcon(BrowserPage *page, int32_t index);
	void UpdateFtpIndicator();
	void OnPreviewChanged();

	/** The theme changed in the settings page. */
	void ApplySettings();
	void UpdateBackground();
	void UpdateAlphaIndex();
	void ToggleAlphaIndex();


private:

	void PushPage(const paf::string &path, bool forceDecrypt = false, bool ownsPsarc = false);
	bool PopPage();
	void FreePage(BrowserPage *page);
	void Refresh();
	BrowserPage *Top() { return m_pages.back(); }
	void UpdateChrome();
	void OpenPreview(int32_t index);
	void FillFileItem(BrowserPage *page, paf::ui::Widget *button, fs::Entry *entry);
	void FillRootItem(BrowserPage *page, paf::ui::Widget *button, fs::Entry *entry);
	void ApplyRowHighlight(BrowserPage *page, int32_t index, paf::ui::Widget *button);
	paf::intrusive_ptr<paf::graph::Surface> Icon(uint32_t hash);
	paf::intrusive_ptr<paf::graph::Surface> VpkIcon(const paf::string &path);

	void OnBack();
	void OnForward();
	void OpenOptionMenu(bool submenu);
	void OpenSortMenu();
	void StartFtp();
	void ReloadRoot();
	void OnOptions();
	void OnItemDecide(paf::ui::Widget *wdg);
	void RequestItemDecide(int32_t index);
	void ApplyItemDecide(int32_t index);
	void OnItemFocus(paf::ui::Widget *wdg);
	void OnMarkToggle(int32_t index, paf::ui::Widget *button = NULL);
	void ApplyMarkToggle(int32_t index, paf::ui::Widget *button);
	void ApplySelectAll();
	void OnOption(int32_t index);
	void OnDialog(int32_t buttonCode);

	void DoAction(Action action);
	void SetClipboard(bool isMove);
	void CollectSelection(paf::vector<paf::string> *out);
	void StartOperation(int32_t type);
	void QueueFinishOperation(int32_t result);
	void StartBackgroundInstall();
	void FinishOperation(int32_t result);
	void FinishUndo(int32_t result);
	void HighlightOptionTarget(bool highlighted);

	enum OperationType
	{
		Op_Copy,
		Op_Move,
		Op_Delete,
		Op_Install,
		Op_InstallBackground,
		Op_Extract,
		Op_CreateZip,
		Op_ExportMedia,
		Op_InstallFolder,
		Op_RefreshLiveArea
	};

	enum UndoType
	{
		Undo_None,
		Undo_Rename,
		Undo_Move,
		Undo_Copy
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
	paf::ui::Widget *m_searchButton;
	paf::ui::Widget *m_ftpIndicator;
	bool m_ftpShown;
	bool m_ftpBusy;
	bool m_musicShown;
	bool m_backPending;
	BrowserPage *m_backPage;
	bool m_markPending;
	BrowserPage *m_markPage;
	int32_t m_markIndex;
	paf::ui::Widget *m_markButton;
	paf::ui::Widget *m_focusedButton;
	bool m_selectAllPending;
	BrowserPage *m_selectAllPage;
	bool m_itemPending;
	BrowserPage *m_itemPage;
	int32_t m_itemIndex;
	bool m_optionsPending;
	bool m_selectButtonPending;
	bool m_selectionActionPending;
	Action m_selectionAction;
	paf::ui::Widget *m_musicIndicator;
	paf::ui::Widget *m_alphaIndex;
	paf::ui::Widget *m_alphaBubble;
	paf::ui::Widget *m_alphaLetter;
	paf::ui::Widget *m_alphaButton;
	paf::ui::Widget *m_selectionBar;
	bool m_searchVisible;
	paf::string m_lastSearchQuery;
	bool m_alphaTouch;
	bool m_alphaVisible;
	bool m_qrDownload;
	int32_t m_alphaCurrent;
	uint32_t m_bgInstallPoll;

	// Fetching an icon's texture once per list cell is a likely cause of slow lists.
	struct CachedIcon
	{
		uint32_t hash;
		paf::intrusive_ptr<paf::graph::Surface> tex;
	};
	CachedIcon m_icons[16];
	uint32_t m_iconCount;
	struct CachedVpkIcon
	{
		paf::string path;
		paf::intrusive_ptr<paf::graph::Surface> tex;
	};
	CachedVpkIcon m_vpkIcons[24];
	uint32_t m_vpkIconCount;

	paf::vector<BrowserPage *> m_pages;
	paf::vector<Action> m_optionActions;

	paf::vector<paf::string> m_clipboard;
	bool m_clipboardIsMove;

	PendingDialog m_pending;
	int32_t m_opType;
	bool m_operationRunning;
	bool m_finishPending;
	int32_t m_operationResult;
	paf::vector<paf::string> m_opSources;
	paf::string m_opTargetDir;
	Action m_inputAction;
	paf::string m_inputTarget;
	paf::string m_installedTitle;
	paf::string m_scratch;
	paf::string m_focusAfterRefresh;
	int32_t m_optionTarget;
	UndoType m_undoType;
	paf::vector<paf::string> m_undoFrom;
	paf::vector<paf::string> m_undoTo;
};

#endif
