#include <sdk_compat.h>
#include <kernel.h>
#include <paf.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <psp2/touch.h>

#include "common.h"
#include "dialog.h"
#include "format.h"
#include "option_menu.h"
#include "text_input.h"
#include "browser_menu.h"
#include "core/fileops.h"
#include "settings.h"
#include "settings_menu.h"
#include "preview_menu.h"
#include "ftp.h"
#include "ftp_menu.h"
#include "text_view.h"
#include "info_menu.h"
#include "notice_menu.h"
#include "music_menu.h"
#include "qr_menu.h"
#include "core/music.h"
#include "core/install.h"
#include "core/bubble.h"
#include "core/pfs.h"
#include "core/psarc.h"
#include "core/download.h"
#include <psp2/net/http.h>
#include <psp2/libssl.h>
#include <psp2/sysmodule.h>

// Second option menu ("More..."): its own page, since the first is still fading out.
#define page_option_menu_sub (0xcd2afa0du)
#define page_option_menu_sort (0x9d6a31c2u)

using namespace paf;

namespace {

	// Progress and cancellation reach the C core through plain function pointers.
	volatile bool s_cancel = false;
	uint32_t s_lastPercent = 0;

	void OnProgress(uint64_t value, uint64_t max)
	{
		uint32_t percent = max ? (uint32_t)(value * 100 / max) : 0;
		if (percent != s_lastPercent)
		{
			s_lastPercent = percent;
			dialog::SetProgress((float)percent);
		}
	}

	int IsCancelled()
	{
		return s_cancel ? 1 : 0;
	}

	class LoadJob : public job::JobItem
	{
	public:

		LoadJob(BrowserMenu *menu, BrowserMenu::BrowserPage *page, bool refresh) :
			job::JobItem("FM::LoadJob", NULL), m_menu(menu), m_page(page), m_refresh(refresh) {}

		int32_t Run()
		{
			m_menu->LoadPage(m_page, m_refresh);
			return SCE_PAF_OK;
		}

		void Finish(int32_t result) {}

	private:

		BrowserMenu *m_menu;
		BrowserMenu::BrowserPage *m_page;
		bool m_refresh;
	};

	class CallJob : public job::JobItem
	{
	public:

		typedef void (BrowserMenu::*Method)();

		CallJob(BrowserMenu *menu, Method method) :
			job::JobItem("FM::CallJob", NULL), m_menu(menu), m_method(method) {}

		int32_t Run()
		{
			(m_menu->*m_method)();
			return SCE_PAF_OK;
		}

		void Finish(int32_t result) {}

	private:

		BrowserMenu *m_menu;
		Method m_method;
	};

	void Enqueue(job::JobItem *item)
	{
		common::SharedPtr<job::JobItem> param(item);
		g_jobQueue->Enqueue(param);
	}

	uint32_t IconFor(const fs::Entry *entry)
	{
		if (entry->iconHash)
		{
			return entry->iconHash;
		}
		switch (entry->kind)
		{
		case fs::Kind_Folder:
			// FMVita marks download folders with their own icon
			return tex_new_folder;
		case fs::Kind_Image:   return tex_ui_image;
		case fs::Kind_Music:   return tex_ui_audio;
		case fs::Kind_Video:   return tex_file_icon_video;
		case fs::Kind_Text:    return tex_ui_text;
		case fs::Kind_Package: return tex_ui_archive;
		case fs::Kind_Vpk:     return tex_ui_vpk;
		case fs::Kind_Sfo:     return tex_ui_sfo;
		default:               return tex_ui_file;
		}	}

	bool Previewable(const fs::Entry *entry)
	{
		if (entry->kind == fs::Kind_Image)
		{
			return true;
		}
		return entry->kind == fs::Kind_Video;
	}

	bool ContainsNoCase(const char *text, const char *needle)
	{
		size_t n = strlen(needle);
		for (; *text; text++)
		{
			if (strncasecmp(text, needle, n) == 0)
			{
				return true;
			}
		}
		return n == 0;
	}

	void OnInstallAnswerCb(bool accepted, void *userdata);

	const float k_rowFile = 80.0f;
	const float k_rowRoot = 96.0f;
	const float k_barWidth = 220.0f;

	void OnSettingsChanged(void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->ApplySettings();
	}

	void BackgroundTask(void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->UpdateBackground();
	}

	void AlphaIndexTask(void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->UpdateAlphaIndex();
	}

	// Timings to tell slow storage from slow UI on the real console.
	void PerfLog(const char *path, uint32_t count, uint32_t usRead)
	{
		SceUID fd = sceIoOpen(FM_DATA_DIR "/perf.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
		if (fd >= 0)
		{
			paf::string line = common::FormatString("%s: %u items, listing %u ms\n", path, count, usRead / 1000);
			sceIoWrite(fd, line.c_str(), line.length());
			sceIoClose(fd);
		}
	}

	paf::wstring ToWide(const paf::string &text)
	{
		paf::wstring out;
		common::Utf8ToUtf16(text, &out);
		return out;
	}
}

BrowserMenu::BrowserMenu() : m_clipboardIsMove(false), m_pending(Pending_None), m_opType(Op_Copy), m_inputAction(Action_Rename),
	m_iconCount(0), m_searchVisible(false), m_alphaTouch(false), m_alphaVisible(false), m_qrDownload(false), m_alphaCurrent(-1), m_optionTarget(-1), m_undoType(Undo_None)
{
	Plugin::PageOpenParam openParam;
	openParam.graphics_flag = 0x80;
	m_root = g_appPlugin->PageOpen(page_browser, openParam);

	m_browserRoot = m_root->FindChild(plane_browser_root_page_browser);
	m_busy = static_cast<ui::BusyIndicator *>(m_root->FindChild(busyindicator_loader_page_browser));
	m_topText = static_cast<ui::Text *>(m_root->FindChild(text_top));
	m_emptyText = static_cast<ui::Text *>(m_root->FindChild(text_empty_page_browser));

	m_background = m_root->FindChild(plane_base_bg_page_browser);
	ApplySettings();
	common::MainThreadCallList::Register(BackgroundTask, this);

	m_emptyText->Hide(common::transition::Type_Reset);
	paf::wstring empty = g_appPlugin->GetString(msg_empty_folder);
	m_emptyText->SetString(empty);

	m_backButton = m_root->FindChild(button_back_page_browser);
	m_backButton->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE);
	m_backButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnBack();
	}, this);

	m_optionsButton = m_root->FindChild(button_options_page_browser);
	m_optionsButton->SetKeycode(inputdevice::pad::Data::PAD_MENU);
	m_optionsButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnOptions();
	}, this);

	// Square marks the focused item. Invisible 1x1 button: only its keycode matters.
	ui::Widget *markButton = m_root->FindChild(button_mark_page_browser);
	markButton->SetKeycode(inputdevice::pad::Data::PAD_DISPLAY);
	markButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		BrowserMenu *menu = reinterpret_cast<BrowserMenu *>(userdata);
		menu->OnMarkToggle(-1);
	}, this);

	// D-pad left goes back like Circle, right opens the focused folder.
	ui::Widget *leftButton = m_root->FindChild(button_left_page_browser);
	leftButton->SetKeycode(inputdevice::pad::Data::PAD_LEFT);
	leftButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnBack();
	}, this);
	ui::Widget *rightButton = m_root->FindChild(button_right_page_browser);
	rightButton->SetKeycode(inputdevice::pad::Data::PAD_RIGHT);
	rightButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnForward();
	}, this);

	// Select: FTP or QR code, as chosen in the settings (FMVita's Select action).
	ui::Widget *ftpButton = m_root->FindChild(button_ftp_page_browser);
	ftpButton->SetKeycode(inputdevice::pad::Data::PAD_SELECT);
	ftpButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnSelectButton();
	}, this);

	// Start opens search; L and R jump to the first and last item.
	ui::Widget *startButton = m_root->FindChild(button_start_page_browser);
	startButton->SetKeycode(inputdevice::pad::Data::PAD_START);
	startButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->ToggleSearch();
	}, this);
	ui::Widget *lButton = m_root->FindChild(button_l_page_browser);
	lButton->SetKeycode(inputdevice::pad::Data::PAD_L);
	lButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->JumpTo(false);
	}, this);
	ui::Widget *rButton = m_root->FindChild(button_r_page_browser);
	rButton->SetKeycode(inputdevice::pad::Data::PAD_R);
	rButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->JumpTo(true);
	}, this);

	// Lower-right corner always opens Options. Triangle uses the same action.
	m_searchButton = m_root->FindChild(button_search_page_browser);
	m_searchButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnOptions();
	}, this);

	m_ftpIndicator = m_root->FindChild(plane_ftp_indicator_page_browser);
	m_ftpIndicator->SetColor(1.0f, 1.0f, 1.0f, 0.5f);
	m_ftpIndicator->Hide(common::transition::Type_Reset);
	m_musicIndicator = m_root->FindChild(button_music_indicator_page_browser);
	m_musicIndicator->Hide(common::transition::Type_Reset);
	m_musicIndicator->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t, ui::Handler *, ui::Event *, void *) { MusicMenu::RestoreMinimized(); }, NULL);
	m_alphaIndex = m_root->FindChild(text_alpha_index_page_browser);
	m_alphaBubble = m_root->FindChild(plane_alpha_bubble_page_browser);
	m_alphaLetter = m_root->FindChild(text_alpha_letter_page_browser);
	m_alphaButton = m_root->FindChild(button_alpha_page_browser);
	m_alphaButton->SetString(ToWide(paf::string("Superior")));
	m_alphaButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->ToggleAlphaIndex();
	}, this);
	m_alphaIndex->SetString(ToWide(paf::string("A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ\n#")));
	m_alphaIndex->Hide(common::transition::Type_Reset);
	m_alphaBubble->Hide(common::transition::Type_Reset);
	m_alphaButton->Hide(common::transition::Type_Reset);
	m_selectionBar = m_root->FindChild(plane_selection_bar_page_browser);
	m_selectionBar->Hide(common::transition::Type_Reset);
	ui::Widget *selectionCancel = m_root->FindChild(button_cancel_selection_page_browser);
	ui::Widget *selectionAll = m_root->FindChild(button_all_selection_page_browser);
	ui::Widget *selectionCopy = m_root->FindChild(button_copy_selection_page_browser);
	selectionCancel->SetString(g_appPlugin->GetString(msg_cancel));
	selectionAll->SetString(g_appPlugin->GetString(msg_select_all));
	selectionCopy->SetString(g_appPlugin->GetString(msg_opt_copy));
	selectionCancel->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->DoActionPublic(Action_ClearSelection);
	}, this);
	selectionAll->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->SelectAll();
	}, this);
	selectionCopy->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->DoActionPublic(Action_Copy);
	}, this);
	common::MainThreadCallList::Register(AlphaIndexTask, this);

	// Search box: filters the list on Enter.
	m_search = static_cast<ui::TextBox *>(m_root->FindChild(text_box_search_page_browser));
	m_search->Hide(common::transition::Type_Reset);
	m_search->AddEventCallback(ui::TextBox::CB_TEXT_BOX_ENTER_PRESSED,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnSearch();
	}, this);

	m_root->AddEventCallback(dialog::DialogEvent,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnDialog(e->GetValue(0));
	}, this);

	m_root->AddEventCallback(OptionMenu::OptionMenuEvent,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		if (e->GetValue(2) == 0)
		{
			BrowserMenu *menu = reinterpret_cast<BrowserMenu *>(userdata);
			menu->HighlightOptionTarget(false);
			if (e->GetValue(0) == OptionMenu::OptionMenuEvent_Button)
				menu->OnOption(e->GetValue(1));
		}
	}, this);

	PushPage(paf::string());
}

BrowserMenu::~BrowserMenu()
{
	common::MainThreadCallList::Unregister(AlphaIndexTask, this);
	common::MainThreadCallList::Unregister(BackgroundTask, this);
	while (PopPage())
	{
	}
	g_appPlugin->PageClose(page_browser, Plugin::PageCloseParam());
}

void BrowserMenu::UpdateBackground()
{
	theme::UpdateBackground(m_background);
	if (MusicMenu::HasMinimized()) m_musicIndicator->Show(common::transition::Type_Reset);
	else m_musicIndicator->Hide(common::transition::Type_Reset);
}

// ------------------------------------------------------------------ pages

void BrowserMenu::UpdateChrome()
{
	BrowserPage *page = Top();

	paf::wstring title;
	if (page->path.empty())
	{
		title = g_appPlugin->GetString(msg_app_title);
	}
	else if (fs::IsRootStyle(page->logicalPath))
	{
		title = g_appPlugin->GetString(msg_system_partitions);
	}
	else
	{
		common::Utf8ToUtf16(page->logicalPath, &title);
	}
	m_topText->SetString(title);

	if (m_pages.size() > 1)
	{
		m_backButton->Show(common::transition::Type_Reset);
	}
	else
	{
		m_backButton->Hide(common::transition::Type_Reset);
	}

	m_searchButton->Show(common::transition::Type_Reset);
	m_selectionBar->Hide(common::transition::Type_Reset);
	const bool musicFolder = !page->logicalPath.empty() &&
		strncasecmp(page->logicalPath.c_str(), "ux0:music", 9) == 0;
	if (musicFolder)
	{
		m_alphaButton->Show(common::transition::Type_FadeinFast);
		m_alphaIndex->Hide(common::transition::Type_Reset);
		m_alphaVisible = false;
	}
	else
	{
		m_alphaButton->Hide(common::transition::Type_Reset);
		m_alphaIndex->Hide(common::transition::Type_Reset);
		m_alphaBubble->Hide(common::transition::Type_Reset);
		m_alphaTouch = false;
		m_alphaCurrent = -1;
		m_alphaVisible = false;
	}
	// Each page starts without the search box; the corner button opens it.
	SetSearchVisible(false);
}

void BrowserMenu::StartListItem(ui::listview::ItemFactory::StartParam &param)
{
	bool mainMenu = false;
	for (uint32_t i = 0; i < m_pages.size(); i++)
	{
		if (m_pages.at(i)->list == param.list_view)
		{
			mainMenu = m_pages.at(i)->path.empty();
			break;
		}
	}
	BrowserPage *owner = NULL;
	for (uint32_t i = 0; i < m_pages.size(); i++)
		if (m_pages.at(i)->list == param.list_view) owner = m_pages.at(i);
	// ListView recycles cells. Replaying SlideFromBottom on a recycled cell leaves
	// several transition layers visible, which looked like duplicated rows.
	param.list_item->Show(common::transition::Type_Reset);
}

void BrowserMenu::ToggleAlphaIndex()
{
	BrowserPage *page = Top();
	if (!page || !page->loaded) return;
	m_alphaVisible = !m_alphaVisible;
	if (m_alphaVisible) m_alphaIndex->Show(common::transition::Type_FadeinFast);
	else
	{
		m_alphaIndex->Hide(common::transition::Type_Reset);
		m_alphaBubble->Hide(common::transition::Type_Reset);
		m_alphaTouch = false;
		m_alphaCurrent = -1;
	}
}

void BrowserMenu::UpdateAlphaIndex()
{
	if (!m_alphaVisible || !m_alphaIndex || !m_alphaIndex->IsVisible() || m_pages.empty()) return;
	BrowserPage *page = Top();
	if (!page || !page->loaded || page->entries.empty() || !page->list || !page->list->IsVisible()) return;
	SceTouchData touch;
	sce_paf_memset(&touch, 0, sizeof(touch));
	sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
	if (!touch.reportNum)
	{
		if (m_alphaTouch) m_alphaBubble->Hide(common::transition::Type_FadeinFast);
		m_alphaTouch = false;
		m_alphaCurrent = -1;
		return;
	}
	const int x = touch.report[0].x;
	const int y = touch.report[0].y;
	if (x > 170 || y < 105 || y > 1015) return;
	int letter = (y - 105) * 27 / 910;
	if (letter < 0) letter = 0;
	if (letter > 26) letter = 26;
	m_alphaTouch = true;
	if (letter == m_alphaCurrent) return;
	m_alphaCurrent = letter;
	char shown[2] = { letter < 26 ? (char)('A' + letter) : '#', 0 };
	m_alphaLetter->SetString(ToWide(paf::string(shown)));
	m_alphaBubble->Show(common::transition::Type_Popup1);
	for (uint32_t i = 0; i < page->entries.size(); i++)
	{
		const char *name = page->entries.at(i)->name.c_str();
		unsigned char first = name[0] ? (unsigned char)toupper((unsigned char)name[0]) : 0;
		bool match = letter < 26 ? first == (unsigned char)('A' + letter) : !isalpha(first);
		if (match)
		{
			// The page may have changed while the touch report was being handled.
			// Never send focus to a list that is no longer the active page.
			if (m_pages.empty() || Top() != page || !page->list->IsVisible()) return;
			page->focused = i;
			page->list->SetFocus(0, i, ui::ListView::FOCUS_ALIGN_TYPE_HEAD, NULL);
			break;
		}
	}
}

void BrowserMenu::SetSearchVisible(bool visible)
{
	BrowserPage *page = Top();
	m_searchVisible = visible && !fs::IsRootStyle(page->path);
	if (m_searchVisible)
	{
		m_search->Show(common::transition::Type_Reset);
		page->list->SetSize(960.0f, 368.0f, 0.0f);
		page->list->SetPos(0.0f, -232.0f, 0.0f);
		m_search->SetFocusedState(true);
		m_search->BeginEdit();
	}
	else
	{
		m_search->ReleaseFocus();
		m_search->Hide(common::transition::Type_Reset);
		page->list->SetSize(960.0f, 448.0f, 0.0f);
		page->list->SetPos(0.0f, -272.0f, 0.0f);
		if (page->focused >= 0 && page->focused < (int32_t)page->entries.size())
		{
			page->list->SetFocus(0, page->focused, ui::ListView::FOCUS_ALIGN_TYPE_CENTER, NULL);
		}
	}
}

void BrowserMenu::ToggleSearch()
{
	BrowserPage *page = Top();
	if (!page->loaded || fs::IsRootStyle(page->path))
	{
		return;
	}
	if (m_searchVisible)
	{
		// closing the search shows the whole folder again
		paf::wstring empty;
		m_search->SetString(empty);
		OnSearch();
	}
	SetSearchVisible(!m_searchVisible);
}

void BrowserMenu::SetRowIcon(BrowserPage *page, int32_t index)
{
	if (index < 0 || index >= (int32_t)page->entries.size())
	{
		return;
	}
	ui::ListItem *item = page->list->GetListItem(0, index);
	if (!item)
	{
		return;
	}
	ui::Widget *button = item->FindChild(image_button_list_item);
	if (!button)
	{
		return;
	}
	intrusive_ptr<graph::Surface> tex = Icon(page->opened == index ? tex_ui_parent : IconFor(page->entries.at(index)));
	if (tex.get())
	{
		button->SetTexture(tex);
	}
}

void BrowserMenu::UpdateFtpIndicator()
{
	if (ftp::IsRunning())
	{
		m_ftpIndicator->Show(common::transition::Type_Reset);
	}
	else
	{
		m_ftpIndicator->Hide(common::transition::Type_Reset);
	}
}

void BrowserMenu::PushPage(const paf::string &path, bool forceDecrypt, bool ownsPsarc)
{
	Plugin::TemplateOpenParam tmpParam;
	paf::string physicalPath = path;
	paf::string logicalPath = path;
	bool ownsMount = false;

	if (!m_pages.empty())
	{
		BrowserPage *current = m_pages.back();
		if (strcmp(current->logicalPath.c_str(), current->path.c_str()) != 0 &&
			strncmp(path.c_str(), current->path.c_str(), current->path.size()) == 0)
		{
			logicalPath = current->logicalPath;
			logicalPath += path.c_str() + current->path.size();
		}

		if ((forceDecrypt || settings::Get().autoDecrypt) && pfsIsProtectedContainer(current->logicalPath.c_str()) && pfsShouldMount(path.c_str()))
		{
			int32_t mountResult = pfsMount(path.c_str());
			if (mountResult < 0 &&
				(!strncasecmp(current->logicalPath.c_str(), "ux0:patch", 9) ||
				 !strncasecmp(current->logicalPath.c_str(), "grw0:patch", 10)))
			{
				const char *slash = strrchr(path.c_str(), '/');
				paf::string title = slash ? slash + 1 : path.c_str();
				mountResult = pfsMount((paf::string("ux0:app/") + title).c_str());
				if (mountResult < 0)
					mountResult = pfsMount((paf::string("gro0:app/") + title).c_str());
			}
			if (mountResult >= 0)
			{
				physicalPath = pfsMountPoint();
				ownsMount = true;
			}
		}
	}

	// The folder being opened shows the "open" icon in the list it came from.
	if (!m_pages.empty())
	{
		BrowserPage *from = m_pages.back();
		from->opened = from->focused;
		SetRowIcon(from, from->opened);
	}

	BrowserPage *page = new BrowserPage();
	page->opened = -1;
	page->path = physicalPath;
	page->logicalPath = logicalPath;
	page->ownsMount = ownsMount;
	page->ownsPsarc = ownsPsarc;
	page->cellCount = 0;
	page->focused = -1;
	page->loaded = false;
	// Animate the whole list, never its recycled cells. This preserves the
	// native page motion without creating duplicated/stacked row layers.
	page->openingAnimation = true;

	g_appPlugin->TemplateOpen(m_browserRoot, template_list_view_generic, tmpParam);
	page->list = static_cast<ui::ListView *>(m_browserRoot->GetChild(m_browserRoot->GetChildrenNum() - 1));
	page->list->SetItemFactory(new ListViewCb(this));
	page->list->InsertSegment(0, 1);
	math::v4 cellSize(960.0f, fs::IsRootStyle(physicalPath) ? k_rowRoot : k_rowFile);
	page->list->SetCellSizeDefault(0, cellSize);
	page->list->SetSegmentLayoutType(0, ui::ListView::LAYOUT_TYPE_LIST);
	page->list->SetPos(0.0f, -272.0f, 0.0f);

	if (!m_pages.empty())
	{
		if (m_pages.size() > 1)
		{
			BrowserPage *older = m_pages.rbegin()[1];
			older->list->Hide(common::transition::Type_Reset);
			older->list->SetTransitionComplete(false);
		}
		BrowserPage *current = m_pages.back();
		current->list->Hide(common::transition::Type_Reset);
		current->list->SetPos(0.0f, -272.0f, 0.0f);
	}

	m_pages.push_back(page);
	UpdateChrome();
	m_emptyText->Hide(common::transition::Type_Reset);
	m_busy->Start();

	Enqueue(new LoadJob(this, page, false));
}

bool BrowserMenu::PopPage()
{
	if (m_pages.empty())
	{
		return false;
	}

	bool isLast = m_pages.size() == 1;
	BrowserPage *page = m_pages.back();

	// The worker fills the entries of this page, so wait for it before freeing them.
	while (!page->loaded)
	{
		thread::Sleep(10);
	}

	page->list->SetName(static_cast<uint32_t>(0));

	if (!isLast)
	{
		page->list->Hide(common::transition::Type_Reset);
	}

	fs::FreeList(&page->all);
	page->entries.clear();
	if (page->ownsMount)
	{
		pfsUmount();
	}
	if (page->ownsPsarc)
	{
		psarcClose();
	}
	delete page;
	m_pages.pop_back();

	if (!isLast)
	{
		BrowserPage *current = m_pages.back();
		int32_t wasOpened = current->opened;
		current->opened = -1;
		SetRowIcon(current, wasOpened);
		current->list->SetPos(0.0f, -272.0f, 0.0f);
		current->list->Hide(common::transition::Type_Reset);
		current->list->Show(common::transition::Type_SlideFromLeft2);
		current->list->SetTransitionComplete(false);
		if (m_pages.size() > 1)
		{
			BrowserPage *older = m_pages.rbegin()[1];
			older->list->Show(common::transition::Type_Reset);
			older->list->SetTransitionComplete(false);
		}

		UpdateChrome();
		if (current->entries.empty())
		{
			m_emptyText->Show(common::transition::Type_Reset);
		}
		else
		{
			m_emptyText->Hide(common::transition::Type_Reset);
		}
	}

	return true;
}

void BrowserMenu::Refresh()
{
	BrowserPage *page = Top();
	page->loaded = false;
	m_busy->Start();
	Enqueue(new LoadJob(this, page, true));
}

void BrowserMenu::LoadPage(BrowserPage *page, bool refresh)
{
	thread::RMutex::MainThreadMutex()->Lock();
	if (refresh && page->cellCount > 0)
	{
		page->list->DeleteCell(0, 0, page->cellCount);
		page->cellCount = 0;
	}
	thread::RMutex::MainThreadMutex()->Unlock();

	fs::FreeList(&page->all);
	page->entries.clear();
	page->focused = -1;

	const uint32_t startTime = sceKernelGetProcessTimeLow();

	int32_t ret = 0;
	if (page->path.empty())
	{
		fs::ListRoot(&page->all);
	}
	else if (fs::IsRootStyle(page->path))
	{
		fs::ListSystem(&page->all);
	}
	else
	{
		ret = fs::ListDirectory(page->path.c_str(), &page->all);
	}
	page->entries = page->all;
	if (page == m_pages.back() && !m_focusAfterRefresh.empty())
	{
		for (uint32_t i = 0; i < page->entries.size(); i++)
		{
			if (!strcasecmp(page->entries.at(i)->name.c_str(), m_focusAfterRefresh.c_str()))
			{
				page->focused = i;
				break;
			}
		}
		m_focusAfterRefresh.clear();
	}

	PerfLog(page->path.c_str(), page->entries.size(), sceKernelGetProcessTimeLow() - startTime);

	thread::RMutex::MainThreadMutex()->Lock();
	m_busy->Stop();
	if (ret < 0)
	{
		dialog::OpenError(g_appPlugin, ret);
	}
	else if (!page->entries.empty())
	{
		page->cellCount = page->entries.size();
		page->list->InsertCell(0, 0, page->cellCount);
		if (page->openingAnimation)
		{
			page->list->Hide(common::transition::Type_Reset);
			page->list->SetPos(0.0f, -272.0f, 0.0f);
			page->list->Show(page->path.empty()
				? common::transition::Type_SlideFromBottom4
				: common::transition::Type_SlideFromRight2);
		}
		if (page->focused >= 0)
			page->list->SetFocus(0, page->focused, ui::ListView::FOCUS_ALIGN_TYPE_CENTER, NULL);
	}
	if (page == m_pages.back())
	{
		if (ret >= 0 && page->entries.empty())
		{
			m_emptyText->Show(common::transition::Type_Reset);
		}
		else
		{
			m_emptyText->Hide(common::transition::Type_Reset);
		}
	}
	thread::RMutex::MainThreadMutex()->Unlock();

	page->loaded = true;
	page->openingAnimation = false;
}

intrusive_ptr<graph::Surface> BrowserMenu::Icon(uint32_t hash)
{
	for (uint32_t i = 0; i < m_iconCount; i++)
	{
		if (m_icons[i].hash == hash)
		{
			return m_icons[i].tex;
		}
	}
	intrusive_ptr<graph::Surface> tex = g_appPlugin->GetTexture(hash);
	if (m_iconCount < sizeof(m_icons) / sizeof(m_icons[0]))
	{
		m_icons[m_iconCount].hash = hash;
		m_icons[m_iconCount].tex = tex;
		m_iconCount++;
	}
	return tex;
}

void BrowserMenu::FillFileItem(BrowserPage *page, ui::Widget *button, fs::Entry *entry)
{
	paf::wstring label = ToWide(entry->name);
	button->SetString(label);
	graph::TextObj *labelObj = static_cast<graph::TextObj *>(button->GetDrawObj(ui::Button::OBJ_LABEL));
	if (labelObj && !label.empty())
	{
		labelObj->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, label.size(), settings::FileFontSize());
	}

	paf::wstring infoWide;
	if (!entry->isDir)
	{
		infoWide = ToWide(format::Size(entry->size));
	}
	button->FindChild(text_list_item_info)->SetString(infoWide);

	// Selection mode: every row shows a check box once anything is marked.
	ui::Widget *check = button->FindChild(plane_list_item_check);
	bool anyMarked = false;
	for (uint32_t i = 0; i < page->entries.size() && !anyMarked; i++)
	{
		anyMarked = page->entries.at(i)->marked;
	}
	if (anyMarked)
	{
		check->SetTexture(Icon(entry->marked ? tex_checkbox_on : tex_checkbox_off), 0, 0);
		button->SetColor(entry->marked ? 0.75f : 1.0f, entry->marked ? 0.42f : 1.0f,
			entry->marked ? 0.95f : 1.0f, 1.0f);
	}
	else
	{
		check->Hide(common::transition::Type_Reset);
		button->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void BrowserMenu::FillRootItem(BrowserPage *page, ui::Widget *button, fs::Entry *entry)
{
	paf::wstring name;
	if (entry->labelHash)
	{
		name = g_appPlugin->GetString(entry->labelHash);
	}
	else
	{
		// user shortcuts are named after their folder
		const char *path = entry->target.c_str();
		const char *slash = strrchr(path, '/');
		const char *colon = strrchr(path, ':');
		const char *base = slash ? slash + 1 : (colon && colon[1] ? colon + 1 : path);
		name = ToWide(paf::string(base));
	}
	button->FindChild(text_device_name)->SetString(name);

	paf::string info = entry->name;
	if (strcmp(entry->target.c_str(), fs::k_ftpPath) == 0 && ftp::IsRunning())
	{
		ftp::Status st;
		ftp::GetStatus(&st);
		info = common::FormatString("ftp://%s:%u", st.ip, st.port);
	}
	if (entry->capacity)
	{
		// The string table holds the wording ("%s livres de %s"), so format it in UTF-8.
		paf::string pattern;
		paf::wstring patternWide = g_appPlugin->GetString(msg_free_of);
		common::Utf16ToUtf8(patternWide, &pattern);
		paf::string freeSize = format::Size(entry->size);
		paf::string totalSize = format::Size(entry->capacity);
		info += "  -  ";
		info += common::FormatString(pattern.c_str(), freeSize.c_str(), totalSize.c_str());
	}
	paf::wstring infoWide = ToWide(info);
	button->FindChild(text_device_info)->SetString(infoWide);

	ui::Widget *bar = button->FindChild(plane_device_bar);
	if (!entry->capacity)
	{
		bar->Hide(common::transition::Type_Reset);
		return;
	}

	const float used = 1.0f - (float)((double)entry->size / (double)entry->capacity);
	ui::Widget *fill = bar->FindChild(plane_device_bar_fill);
	fill->SetSize(k_barWidth * used, 8.0f, 0.0f);
	if (used > 0.9f)
	{
		fill->SetColor(1.0f, 0.35f, 0.3f, 1.0f); // nearly full
	}
	else
	{
		const float *accent = theme::Accent();
		fill->SetColor(accent[0], accent[1], accent[2], 1.0f);
	}
}

void BrowserMenu::ApplySettings()
{
	theme::ApplyBackground(m_background);

	// Storage bars and file-name sizes take their settings when cells are built.
	if (!m_pages.empty())
	{
		for (uint32_t i = 0; i < m_pages.size(); i++)
		{
			BrowserPage *page = m_pages.at(i);
			if (page->cellCount > 0 && page->loaded)
			{
				page->list->DeleteCell(0, 0, page->cellCount);
				page->list->InsertCell(0, 0, page->cellCount);
			}
		}
	}
}

void BrowserMenu::OpenPreview(int32_t index)
{
	BrowserPage *page = Top();
	paf::vector<paf::string> paths;
	int32_t start = 0;
	for (int32_t i = 0; i < (int32_t)page->entries.size(); i++)
	{
		fs::Entry *entry = page->entries.at(i);
		if (Previewable(entry))
		{
			if (i == index)
			{
				start = paths.size();
			}
			paths.push_back(fs::Join(page->path, entry->name));
		}
	}
	new PreviewMenu(m_root, paths, start);
}

ui::ListItem *BrowserMenu::CreateListItem(ui::listview::ItemFactory::CreateParam &param)
{
	Plugin::TemplateOpenParam tmpParam;

	// A list being destroyed is renamed to 0: give it an empty placeholder.
	if (!param.list_view->GetName().GetIDHash())
	{
		return new ui::ListItem(param.parent, 0);
	}

	// Lists under the top page can still request cells during a transition, so
	// resolve the page from the list itself, never from the top of the stack.
	BrowserPage *page = NULL;
	for (uint32_t i = 0; i < m_pages.size(); i++)
	{
		if (m_pages.at(i)->list == param.list_view)
		{
			page = m_pages.at(i);
			break;
		}
	}
	if (!page || param.cell_index < 0 || param.cell_index >= (int32_t)page->entries.size())
	{
		return new ui::ListItem(param.parent, 0);
	}

	fs::Entry *entry = page->entries.at(param.cell_index);

	const bool rootStyle = fs::IsRootStyle(page->path);

	ui::Widget *targetRoot = param.parent;
	g_appPlugin->TemplateOpen(targetRoot, rootStyle ? template_list_item_device : template_list_item_file, tmpParam);
	ui::Widget *item = targetRoot->GetChild(targetRoot->GetChildrenNum() - 1);

	ui::Widget *button = item->FindChild(image_button_list_item);
	button->SetName(param.cell_index);

	const float rowH = rootStyle ? k_rowRoot : k_rowFile;
	item->SetSize(960.0f, rowH, 0.0f);
	button->SetSize(840.0f, rowH, 0.0f);

	if (rootStyle)
	{
		FillRootItem(page, button, entry);
	}
	else
	{
		FillFileItem(page, button, entry);
	}

	intrusive_ptr<graph::Surface> tex = Icon(page->opened == param.cell_index ? tex_ui_parent : IconFor(entry));
	if (tex.get())
	{
		button->SetTexture(tex);
	}

	button->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnItemDecide(static_cast<ui::Widget *>(self));
	}, this);

	button->AddEventCallback(ui::Handler::CB_FOCUS_IN,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnItemFocus(static_cast<ui::Widget *>(self));
	}, this);

	// Touch has no focus, so a press also picks the item the options act on.
	button->AddEventCallback(ui::Button::CB_BTN_PRESS,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnItemFocus(static_cast<ui::Widget *>(self));
	}, this);

	// A long press selects.
	button->AddEventCallback(ui::Button::CB_BTN_LONG_PRESS,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		ui::Widget *wdg = static_cast<ui::Widget *>(self);
		reinterpret_cast<BrowserMenu *>(userdata)->OnMarkToggle(wdg->GetName().GetIDHash());
	}, this);

	return static_cast<ui::ListItem *>(item);
}

// ------------------------------------------------------------------ input

void BrowserMenu::OnBack()
{
	// PopPage waits for the worker, and the worker needs the main thread mutex to
	// finish loading: never wait for it from here, just ignore the press.
	if (m_pages.size() > 1 && Top()->loaded)
	{
		PopPage();
	}
}

void BrowserMenu::DoActionPublic(Action action)
{
	DoAction(action);
}

void BrowserMenu::OnSelectButton()
{
	if (dialog::IsActive())
	{
		return;
	}
	DoAction(settings::Get().selectFunction == 1 ? Action_Qr : Action_Ftp);
}

void BrowserMenu::JumpTo(bool end)
{
	BrowserPage *page = Top();
	if (!page->loaded || page->entries.empty())
	{
		return;
	}
	int32_t index = end ? (int32_t)page->entries.size() - 1 : 0;
	page->list->SetFocus(0, index, end ? ui::ListView::FOCUS_ALIGN_TYPE_TAIL : ui::ListView::FOCUS_ALIGN_TYPE_HEAD, NULL);
	page->focused = index;
}

void BrowserMenu::OnSearch()
{
	BrowserPage *page = Top();
	if (!page->loaded)
	{
		return;
	}
	paf::wstring text;
	m_search->GetString(text);
	paf::string query;
	common::Utf16ToUtf8(text, &query);
	m_search->EndEdit();

	if (page->cellCount > 0)
	{
		page->list->DeleteCell(0, 0, page->cellCount);
	}
	page->entries.clear();
	for (uint32_t i = 0; i < page->all.size(); i++)
	{
		fs::Entry *e = page->all.at(i);
		if (query.empty() || ContainsNoCase(e->name.c_str(), query.c_str()))
		{
			page->entries.push_back(e);
		}
	}
	page->focused = -1;
	page->cellCount = page->entries.size();
	if (page->cellCount > 0)
	{
		page->list->InsertCell(0, 0, page->cellCount);
		m_emptyText->Hide(common::transition::Type_Reset);
	}
	else
	{
		m_emptyText->Show(common::transition::Type_Reset);
	}
}

void BrowserMenu::OnForward()
{
	BrowserPage *page = Top();
	if (!page->loaded || page->focused < 0 || page->focused >= (int32_t)page->entries.size())
	{
		return;
	}
	fs::Entry *entry = page->entries.at(page->focused);
	if (entry->isDir)
	{
		PushPage(fs::IsRootStyle(page->path) ? entry->target : fs::Join(page->path, entry->name));
	}
}

void BrowserMenu::OnItemFocus(ui::Widget *wdg)
{
	Top()->focused = wdg->GetName().GetIDHash();
}

void BrowserMenu::OnItemDecide(ui::Widget *wdg)
{
	BrowserPage *page = Top();
	if (!page->loaded)
	{
		return;
	}

	int32_t index = wdg->GetName().GetIDHash();
	page->focused = index;
	fs::Entry *entry = page->entries.at(index);

	if (strcmp(entry->target.c_str(), fs::k_ftpPath) == 0)
	{
		StartFtp();
	}
	else if (entry->isDir)
	{
		PushPage(fs::IsRootStyle(page->path) ? entry->target : fs::Join(page->path, entry->name));
	}
	else if (entry->kind == fs::Kind_Package && strcasecmp(strrchr(entry->name.c_str(), '.') ? strrchr(entry->name.c_str(), '.') : "", ".psarc") == 0)
	{
		paf::string archivePath = fs::Join(page->path, entry->name);
		int32_t result = psarcOpen(archivePath.c_str());
		if (result >= 0)
			PushPage(archivePath, false, true);
		else
			dialog::OpenError(g_appPlugin, result);
	}
	else if (Previewable(entry))
	{
		OpenPreview(index);
	}
	else if (entry->kind == fs::Kind_Music && musicSupported(entry->name.c_str()))
	{
		// the folder's songs, starting at this one
		paf::vector<paf::string> songs;
		int32_t start = 0;
		for (int32_t i = 0; i < (int32_t)page->entries.size(); i++)
		{
			fs::Entry *e = page->entries.at(i);
			if (e->kind == fs::Kind_Music && musicSupported(e->name.c_str()))
			{
				if (i == index)
				{
					start = songs.size();
				}
				songs.push_back(fs::Join(page->path, e->name));
			}
		}
		new MusicMenu(m_root, songs, start);
	}
	else if (entry->kind == fs::Kind_Vpk)
	{
		m_opSources.clear();
		m_opSources.push_back(fs::Join(page->path, entry->name));
		new InfoMenu(m_root, InfoMenu::Mode_Install, m_opSources.at(0), entry, OnInstallAnswerCb, this);
	}
	else if (entry->kind == fs::Kind_Text || entry->kind == fs::Kind_Sfo || entry->kind == fs::Kind_Other || entry->kind == fs::Kind_Plugin)
	{
		new TextView(m_root, fs::Join(page->path, entry->name).c_str());
	}
	else
	{
		DoAction(Action_Info);
	}
}

void BrowserMenu::OnMarkToggle(int32_t index)
{
	BrowserPage *page = Top();
	if (index < 0)
	{
		index = page->focused;
	}
	if (index < 0 || index >= (int32_t)page->entries.size())
	{
		return;
	}

	fs::Entry *entry = page->entries.at(index);
	if (fs::IsRootStyle(page->path))
	{
		return;
	}
	entry->marked = !entry->marked;
	page->focused = index;

	// Refresh the boxes of the rows that exist: entering or leaving selection mode
	// changes all of them.
	bool anyMarked = false;
	for (uint32_t i = 0; i < page->entries.size() && !anyMarked; i++)
	{
		anyMarked = page->entries.at(i)->marked;
	}
	for (uint32_t i = 0; i < page->entries.size(); i++)
	{
		ui::ListItem *item = page->list->GetListItem(0, i);
		if (!item)
		{
			continue;
		}
		ui::Widget *check = item->FindChild(plane_list_item_check);
		if (anyMarked)
		{
			check->SetTexture(Icon(page->entries.at(i)->marked ? tex_checkbox_on : tex_checkbox_off), 0, 0);
			check->Show(common::transition::Type_Reset);
			item->FindChild(image_button_list_item)->SetColor(page->entries.at(i)->marked ? 0.75f : 1.0f,
				page->entries.at(i)->marked ? 0.42f : 1.0f, page->entries.at(i)->marked ? 0.95f : 1.0f, 1.0f);
		}
		else
		{
			check->Hide(common::transition::Type_Reset);
			item->FindChild(image_button_list_item)->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}
	if (anyMarked)
		m_selectionBar->Show(common::transition::Type_SlideFromBottom1);
	else
		m_selectionBar->Hide(common::transition::Type_SlideFromBottom1);
}

void BrowserMenu::SelectAll()
{
	BrowserPage *page = Top();
	if (!page || fs::IsRootStyle(page->path)) return;
	for (uint32_t i = 0; i < page->entries.size(); i++)
		page->entries.at(i)->marked = true;
	// Rebuild visible cells once so checks and the Music-style tint stay in sync.
	if (page->cellCount)
	{
		page->list->DeleteCell(0, 0, page->cellCount);
		page->list->InsertCell(0, 0, page->cellCount);
	}
	m_selectionBar->Show(common::transition::Type_Reset);
}

void BrowserMenu::OnOptions()
{
	BrowserPage *page = Top();
	if (!page->loaded || dialog::IsActive())
	{
		return;
	}
	OpenOptionMenu(false);
}

void BrowserMenu::OpenOptionMenu(bool submenu)
{
	BrowserPage *page = Top();
	const bool atRoot = fs::IsRootStyle(page->path);
	const bool hasTarget = page->focused >= 0 && page->focused < (int32_t)page->entries.size();
	fs::Entry *target = hasTarget ? page->entries.at(page->focused) : NULL;
	bool anyMarked = false;
	for (uint32_t i = 0; i < page->entries.size(); i++)
	{
		anyMarked |= page->entries.at(i)->marked;
	}
	if (!submenu)
	{
		m_optionTarget = hasTarget ? page->focused : -1;
		HighlightOptionTarget(true);
	}

	// At most 8 rows fit the screen; the rest goes to "More...".
	m_optionActions.clear();
	if (submenu)
	{
		if (anyMarked)
		{
			m_optionActions.push_back(Action_ClearSelection);
		}
		if (target && target->isDir)
		{
			m_optionActions.push_back(Action_AddShortcut);
		}
		m_optionActions.push_back(Action_Ftp);
		m_optionActions.push_back(Action_Qr);
		m_optionActions.push_back(Action_Sort);
		m_optionActions.push_back(Action_Settings);
		if (m_undoType != Undo_None) m_optionActions.push_back(Action_Undo);
	}
	else if (atRoot)
	{
		const bool special = target && (strcmp(target->target.c_str(), fs::k_systemPath) == 0 || strcmp(target->target.c_str(), fs::k_ftpPath) == 0);
		if (target && !special)
		{
			m_optionActions.push_back(Action_Info);
		}
		if (target && target->isShortcut)
		{
			m_optionActions.push_back(Action_RemoveShortcut);
		}
		m_optionActions.push_back(Action_Ftp);
		m_optionActions.push_back(Action_Qr);
		m_optionActions.push_back(Action_Sort);
		m_optionActions.push_back(Action_Settings);
		if (m_undoType != Undo_None) m_optionActions.push_back(Action_Undo);
	}
	else
	{
		if (target || anyMarked)
		{
			m_optionActions.push_back(Action_Copy);
			m_optionActions.push_back(Action_Move);
			m_optionActions.push_back(Action_Delete);
		}
		if (!m_clipboard.empty())
		{
			m_optionActions.push_back(Action_Paste);
		}
		if (target && !anyMarked)
		{
			m_optionActions.push_back(Action_Rename);
		}
		if (target && target->isDir && !anyMarked && pfsShouldMount(fs::Join(page->path, target->name).c_str()))
		{
			m_optionActions.push_back(Action_Decrypt);
		}
		m_optionActions.push_back(Action_NewFolder);
		if (target && !anyMarked)
		{
			m_optionActions.push_back(Action_Info);
		}
		m_optionActions.push_back(Action_More);
	}

	static const uint32_t k_labels[] = {
		msg_opt_select, msg_opt_clear_selection, msg_opt_copy, msg_opt_move, msg_opt_paste,
		msg_opt_delete, msg_opt_rename, msg_opt_new_folder, msg_opt_info, msg_opt_settings,
		msg_opt_more, msg_opt_add_shortcut, msg_opt_remove_shortcut, msg_ftp_server, msg_qr_code,
		msg_sort, msg_sort_date, msg_sort_name, msg_sort_type, msg_decrypt, msg_undo
	};

	paf::vector<OptionMenu::Button> buttons;
	for (uint32_t i = 0; i < m_optionActions.size(); i++)
	{
		OptionMenu::Button bt;
		bt.label = g_appPlugin->GetString(k_labels[m_optionActions.at(i)]);
		buttons.push_back(bt);
	}

	new OptionMenu(g_appPlugin, m_root, &buttons, submenu ? page_option_menu_sub : 0xd098335eu);
}

void BrowserMenu::HighlightOptionTarget(bool highlighted)
{
	if (m_pages.empty() || m_optionTarget < 0) return;
	BrowserPage *page = Top();
	ui::ListItem *item = page->list->GetListItem(0, m_optionTarget);
	if (!item) return;
	ui::Widget *button = item->FindChild(image_button_list_item);
	if (!button) return;
	button->SetColor(highlighted ? 0.72f : 1.0f, highlighted ? 0.38f : 1.0f,
		highlighted ? 0.92f : 1.0f, 1.0f);
	if (!highlighted) m_optionTarget = -1;
}

void BrowserMenu::OpenSortMenu()
{
	m_optionActions.clear();
	m_optionActions.push_back(Action_SortDate);
	m_optionActions.push_back(Action_SortName);
	m_optionActions.push_back(Action_SortType);
	static const uint32_t labels[] = { msg_sort_date, msg_sort_name, msg_sort_type };
	paf::vector<OptionMenu::Button> buttons;
	for (uint32_t i = 0; i < 3; i++)
	{
		OptionMenu::Button button;
		button.label = g_appPlugin->GetString(labels[i]);
		buttons.push_back(button);
	}
	new OptionMenu(g_appPlugin, m_root, &buttons, page_option_menu_sort);
}

void BrowserMenu::OnOption(int32_t index)
{
	if (index >= 0 && index < (int32_t)m_optionActions.size())
	{
		DoAction(m_optionActions.at(index));
	}
}

// ------------------------------------------------------------------ actions

void BrowserMenu::CollectSelection(paf::vector<paf::string> *out)
{
	BrowserPage *page = Top();
	for (uint32_t i = 0; i < page->entries.size(); i++)
	{
		if (page->entries.at(i)->marked)
		{
			out->push_back(fs::Join(page->path, page->entries.at(i)->name));
		}
	}
	if (out->empty() && page->focused >= 0 && page->focused < (int32_t)page->entries.size())
	{
		out->push_back(fs::Join(page->path, page->entries.at(page->focused)->name));
	}
}

void BrowserMenu::SetClipboard(bool isMove)
{
	m_clipboard.clear();
	CollectSelection(&m_clipboard);
	m_clipboardIsMove = isMove;
}

namespace {
	void OnInstallAnswerCb(bool accepted, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnInstallAnswer(accepted);
	}
}

void BrowserMenu::OnInstallAnswer(bool accepted)
{
	if (accepted)
	{
		StartOperation(Op_Install);
	}
}

namespace {
	void OnInputDone(const paf::wstring &text, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnNameEntered(text);
	}
}

void BrowserMenu::DoAction(Action action)
{
	BrowserPage *page = Top();

	switch (action)
	{
	case Action_Select:
		OnMarkToggle(-1);
		break;

	case Action_ClearSelection:
		for (uint32_t i = 0; i < page->entries.size(); i++)
		{
			if (page->entries.at(i)->marked)
			{
				OnMarkToggle(i);
			}
		}
		break;

	case Action_Copy:
		SetClipboard(false);
		break;

	case Action_Move:
		SetClipboard(true);
		break;

	case Action_Paste:
		m_opSources = m_clipboard;
		m_opTargetDir = page->path;
		StartOperation(m_clipboardIsMove ? Op_Move : Op_Copy);
		break;

	case Action_Delete:
		m_opSources.clear();
		CollectSelection(&m_opSources);
		m_pending = Pending_ConfirmDelete;
		m_scratch.clear();
		for (uint32_t i = 0; i < m_opSources.size() && i < 4; i++)
		{
			const char *path = m_opSources.at(i).c_str();
			const char *slash = strrchr(path, '/');
			if (i) m_scratch += "\n";
			m_scratch += slash ? slash + 1 : path;
		}
		if (m_opSources.size() > 4) m_scratch += common::FormatString("\n+%u", (uint32_t)m_opSources.size() - 4);
		{
			paf::string pattern;
			common::Utf16ToUtf8(paf::wstring(g_appPlugin->GetString(msg_confirm_delete_item)), &pattern);
			paf::wstring question = ToWide(common::FormatString(pattern.c_str(), m_scratch.c_str()));
			dialog::OpenYesNo(g_appPlugin, g_appPlugin->GetString(msg_confirm_delete_title), question.c_str());
		}
		break;

	case Action_Rename:
	{
		fs::Entry *entry = page->entries.at(page->focused);
		m_inputAction = Action_Rename;
		m_inputTarget = entry->name;
		new TextInput(m_root, g_appPlugin->GetString(msg_rename_title), ToWide(entry->name), OnInputDone, this);
		break;
	}

	case Action_NewFolder:
		m_inputAction = Action_NewFolder;
		new TextInput(m_root, g_appPlugin->GetString(msg_new_folder_title), paf::wstring(), OnInputDone, this);
		break;

	case Action_Settings:
		new SettingsMenu(m_root, OnSettingsChanged, this);
		break;

	case Action_More:
		OpenOptionMenu(true);
		break;

	case Action_AddShortcut:
		if (page->focused >= 0)
		{
			shortcuts::Add(fs::Join(page->path, page->entries.at(page->focused)->name));
			m_pages.at(0)->loaded = false;
			ReloadRoot();
		}
		break;

	case Action_RemoveShortcut:
		if (page->focused >= 0)
		{
			shortcuts::Remove(page->entries.at(page->focused)->target);
			Refresh();
		}
		break;

	case Action_Ftp:
		StartFtp();
		break;

	case Action_Qr:
		new QrMenu(m_root,
		[](const paf::string &payload, void *userdata)
		{
			reinterpret_cast<BrowserMenu *>(userdata)->OnQrResult(payload);
		}, this);
		break;

	case Action_Undo:
		dialog::OpenPleaseWait(g_appPlugin, NULL, g_appPlugin->GetString(msg_undoing));
		Enqueue(new CallJob(this, &BrowserMenu::RunUndo));
		break;

	case Action_Sort:
		OpenSortMenu();
		break;

	case Action_SortDate:
	case Action_SortName:
	case Action_SortType:
		settings::Get().sortMode = action == Action_SortDate ? 1 : (action == Action_SortType ? 2 : 0);
		settings::Save();
		Refresh();
		break;

	case Action_Decrypt:
		if (page->focused >= 0)
			PushPage(fs::Join(page->path, page->entries.at(page->focused)->name), true);
		break;

	case Action_Info:
		if (!fs::IsRootStyle(page->path) && page->focused >= 0 && !page->entries.at(page->focused)->isDir)
		{
			fs::Entry *entry = page->entries.at(page->focused);
			new InfoMenu(m_root, InfoMenu::Mode_Info, fs::Join(page->path, entry->name), entry, NULL, NULL);
			break;
		}
		m_pending = Pending_Message;
		dialog::OpenPleaseWait(g_appPlugin, NULL, g_appPlugin->GetString(msg_wait));
		Enqueue(new CallJob(this, &BrowserMenu::RunInfo));
		break;
	}
}

void BrowserMenu::OnNameEntered(const paf::wstring &text)
{
	if (text.empty())
	{
		return;
	}

	paf::string name;
	common::Utf16ToUtf8(text, &name);
	if (!fs::IsValidName(name.c_str()))
	{
		dialog::OpenError(g_appPlugin, SCE_ERROR_ERRNO_EINVAL);
		return;
	}

	BrowserPage *page = Top();
	int32_t ret;
	if (m_inputAction == Action_Rename)
	{
		paf::string oldPath = fs::Join(page->path, m_inputTarget);
		paf::string newPath = fs::Join(page->path, name);
		ret = sceIoRename(oldPath.c_str(), newPath.c_str());
		if (ret >= 0)
		{
			m_undoType = Undo_Rename;
			m_undoFrom.clear(); m_undoTo.clear();
			m_undoFrom.push_back(oldPath); m_undoTo.push_back(newPath);
			m_focusAfterRefresh = name;
		}
	}
	else
	{
		ret = sceIoMkdir(fs::Join(page->path, name).c_str(), 0777);
	}

	if (ret < 0)
	{
		dialog::OpenError(g_appPlugin, ret);
		return;
	}
	Refresh();
}

void BrowserMenu::RunInfo()
{
	BrowserPage *page = Top();
	fs::Entry *entry = page->entries.at(page->focused);

	paf::string text;
	const paf::string fullPath = fs::IsRootStyle(page->path) ? entry->target : fs::Join(page->path, entry->name);
	text = fullPath;
	text += "\n";

	paf::wstring message = ToWide(text);
	if (entry->kind == fs::Kind_Device)
	{
		message += g_appPlugin->GetString(msg_info_size);
		message += L": ";
		message += ToWide(format::Size(entry->capacity));
		message += L"\n";
		message += g_appPlugin->GetString(msg_info_free);
		message += L": ";
		message += ToWide(format::Size(entry->size));
	}
	else
	{
		uint64_t size = 0;
		uint32_t folders = 0, files = 0;
		getPathInfo(fullPath.c_str(), &size, &folders, &files, NULL);

		message += g_appPlugin->GetString(msg_info_size);
		message += L": ";
		message += ToWide(format::Size(size));
		if (entry->isDir)
		{
			message += L"\n";
			message += g_appPlugin->GetString(msg_info_folders);
			message += L": ";
			message += ToWide(common::FormatString("%u", folders ? folders - 1 : 0));
			message += L"\n";
			message += g_appPlugin->GetString(msg_info_files);
			message += L": ";
			message += ToWide(common::FormatString("%u", files));
		}
	}

	dialog::Close();
	dialog::OpenOk(g_appPlugin, g_appPlugin->GetString(msg_info_title), message.c_str());
}

void BrowserMenu::StartOperation(int32_t type)
{
	const uint32_t title = type == Op_Copy ? msg_copying : (type == Op_Move ? msg_moving : (type == Op_Install ? msg_installing : msg_deleting));

	m_opType = type;
	m_pending = Pending_Operation;
	s_cancel = false;
	s_lastPercent = 0;

	paf::wstring progressText = g_appPlugin->GetString(title);
	if ((type == Op_Copy || type == Op_Move) && !m_opSources.empty())
	{
		const char *path = m_opSources.at(0).c_str();
		const char *slash = strrchr(path, '/');
		paf::string pattern;
		common::Utf16ToUtf8(paf::wstring(g_appPlugin->GetString(type == Op_Move ? msg_moving_file : msg_copying_file)), &pattern);
		progressText = ToWide(common::FormatString(pattern.c_str(), slash ? slash + 1 : path));
		m_undoFrom.clear();
		m_undoTo.clear();
		m_undoType = Undo_None;
	}
	else if (type == Op_Install && !m_opSources.empty())
	{
		const char *path = m_opSources.at(0).c_str();
		const char *slash = strrchr(path, '/');
		progressText += L"\n";
		progressText += ToWide(slash ? slash + 1 : path);
	}
	dialog::OpenProgress(g_appPlugin, NULL, progressText.c_str());
	if (type == Op_Install && !m_opSources.empty())
	{
		VpkInfo info;
		memset(&info, 0, sizeof(info));
		if (readVpkInfo(m_opSources.at(0).c_str(), &info) >= 0 && info.icon)
		{
			void *bubble = NULL;
			uint32_t bubbleSize = 0;
			if (makeBubblePng(info.icon, info.iconSize, &bubble, &bubbleSize) >= 0)
			{
				int32_t res = -1;
				common::SharedPtr<MemFile> file = MemFile::Open(bubble, bubbleSize, &res);
				if (res == SCE_PAF_OK)
					dialog::SetProgressIcon(graph::Surface::Load(gutil::GetDefaultSurfacePool(), (common::SharedPtr<File> &)file));
				free(bubble);
			}
			free(info.icon);
		}
	}
	Enqueue(new CallJob(this, &BrowserMenu::RunOperation));
}

void BrowserMenu::RunOperation()
{
	if (m_opType == Op_Install)
	{
		uint64_t value = 0;
		FileProcessParam param;
		param.value = &value;
		int64_t total = archiveTotalSize(m_opSources.at(0).c_str());
		param.max = total > 0 ? (uint64_t)total : 1;
		param.SetProgress = OnProgress;
		param.cancelHandler = IsCancelled;
		char titleid[12] = { 0 };
		int32_t result = installVpk(m_opSources.at(0).c_str(), &param, titleid);
		m_installedTitle = titleid;
		FinishOperation(result);
		return;
	}

	// Copies and deletions report against a total measured up front.
	uint64_t total = 0;
	uint32_t folders = 0, files = 0;
	for (uint32_t i = 0; i < m_opSources.size() && m_opType != Op_Move; i++)
	{
		getPathInfo(m_opSources.at(i).c_str(), &total, &folders, &files, NULL);
	}

	uint64_t value = 0;
	FileProcessParam param;
	param.value = &value;
	param.max = m_opType == Op_Delete ? (uint64_t)(folders + files) : total + (uint64_t)folders * DIRECTORY_SIZE;
	param.SetProgress = OnProgress;
	param.cancelHandler = IsCancelled;

	int32_t result = 1;
	for (uint32_t i = 0; i < m_opSources.size() && result > 0 && !s_cancel; i++)
	{
		const char *src = m_opSources.at(i).c_str();

		if (m_opType == Op_Delete)
		{
			result = removePath(src, &param);
			continue;
		}

		const char *slash = strrchr(src, '/');
		const char *colon = strrchr(src, ':');
		const char *base = slash ? slash + 1 : (colon ? colon + 1 : src);
		paf::string dst = fs::Join(m_opTargetDir, fs::UniqueName(m_opTargetDir, paf::string(base)));

		if (m_opType == Op_Copy)
		{
			result = copyPath(src, dst.c_str(), &param);
		}
		else
		{
			result = movePath(src, dst.c_str(), 0, &param);
			if (result == 1)
			{
				SceIoStat stat;
				if (sceIoGetstat(src, &stat) >= 0)
				{
					// sceIoRename cannot cross devices: copy, then remove the source.
					result = copyPath(src, dst.c_str(), &param);
					if (result > 0)
					{
						result = removePath(src, NULL);
					}
				}
			}
		}
		if (result > 0)
		{
			m_undoFrom.push_back(src);
			m_undoTo.push_back(dst);
		}
	}

	FinishOperation(result);
}

void BrowserMenu::FinishOperation(int32_t result)
{
	m_pending = Pending_None;
	dialog::Close();

	if (result >= 0 && m_opType == Op_Move)
	{
		m_clipboard.clear();
	}
	if (result > 0 && (m_opType == Op_Move || m_opType == Op_Copy) && !m_undoTo.empty())
		m_undoType = m_opType == Op_Move ? Undo_Move : Undo_Copy;

	thread::RMutex::MainThreadMutex()->Lock();
	if (result < 0)
	{
		dialog::OpenError(g_appPlugin, result);
	}
	else if (result > 0 && m_opType == Op_Install)
	{
		// Same panel as the prompt: the bubble, the app's name, OK / Run.
		fs::Entry entry;
		entry.kind = fs::Kind_Vpk;
		entry.isDir = false;
		entry.marked = false;
		SceIoStat stat;
		entry.size = sceIoGetstat(m_opSources.at(0).c_str(), &stat) >= 0 ? stat.st_size : 0;
		entry.capacity = 0;
		entry.labelHash = 0;
		entry.iconHash = 0;
		entry.isShortcut = false;
		new InfoMenu(m_root, InfoMenu::Mode_Installed, m_opSources.at(0), &entry,
		[](bool run, void *userdata)
		{
			BrowserMenu *menu = reinterpret_cast<BrowserMenu *>(userdata);
			if (run && !menu->InstalledTitle().empty())
			{
				// As FMVita: start the new app and leave.
				paf::string uri = common::FormatString("psgm:play?titleid=%s", menu->InstalledTitle().c_str());
				sceAppMgrLaunchAppByUri(0xFFFFF, uri.c_str());
				sceKernelExitProcess(0);
			}
		}, this);
	}
	Refresh();
	thread::RMutex::MainThreadMutex()->Unlock();
}

void BrowserMenu::RunUndo()
{
	int32_t result = 1;
	for (int32_t i = (int32_t)m_undoTo.size() - 1; i >= 0 && result > 0; i--)
	{
		if (m_undoType == Undo_Copy)
			result = removePath(m_undoTo.at(i).c_str(), NULL);
		else
			result = movePath(m_undoTo.at(i).c_str(), m_undoFrom.at(i).c_str(), 0, NULL);
	}
	FinishUndo(result);
}

void BrowserMenu::FinishUndo(int32_t result)
{
	thread::RMutex::MainThreadMutex()->Lock();
	dialog::Close();
	if (result < 0)
		dialog::OpenError(g_appPlugin, result);
	else
	{
		m_undoType = Undo_None;
		m_undoFrom.clear();
		m_undoTo.clear();
		Refresh();
		if (m_pages.size() > 1) ReloadRoot();
	}
	thread::RMutex::MainThreadMutex()->Unlock();
}

namespace {
	void OnFtpMenuClosed(bool stopped, void *userdata)
	{
		reinterpret_cast<BrowserMenu *>(userdata)->OnFtpClosed(stopped);
	}
}

void BrowserMenu::StartFtp()
{
	if (dialog::IsActive())
	{
		return;
	}
	int32_t ret = ftp::Start();
	if (ret < 0)
	{
		new NoticeMenu(m_root, msg_wifi_off_title, msg_wifi_off, msg_network_settings, msg_cancel,
		[](int32_t choice, void *userdata)
		{
			if (choice == 0)
			{
				// Opens the system Settings, where Network holds the Wi-Fi options.
				sceAppMgrLaunchAppByUri(0xFFFFF, "settings_dlg:");
			}
		}, this);
		return;
	}
	UpdateFtpIndicator();
	new FtpMenu(m_root, OnFtpMenuClosed, this);
}

void BrowserMenu::OnFtpClosed(bool stopped)
{
	// Files may have changed over FTP; the root also shows whether the server runs.
	UpdateFtpIndicator();
	Refresh();
	if (m_pages.size() > 1)
	{
		ReloadRoot();
	}
}

void BrowserMenu::OnQrResult(const paf::string &payload)
{
	m_scratch = payload;
	if (!strncasecmp(payload.c_str(), "http://", 7) || !strncasecmp(payload.c_str(), "https://", 8))
	{
		const char *end = strchr(payload.c_str(), '?');
		const char *slash = strrchr(payload.c_str(), '/');
		const char *dot = slash ? strrchr(slash, '.') : NULL;
		m_qrDownload = dot && (!end || dot < end) && strncasecmp(dot, ".html", 5) && strncasecmp(dot, ".htm", 4);
		m_pending = Pending_QrOpen;
		paf::wstring text = ToWide(payload);
		dialog::OpenYesNo(g_appPlugin, g_appPlugin->GetString(msg_qr_open_title), text.c_str());
	}
	else
	{
		paf::wstring text = ToWide(payload);
		dialog::OpenOk(g_appPlugin, g_appPlugin->GetString(msg_qr_result), text.c_str());
	}
}

void BrowserMenu::RunQrDownload()
{
	const char *url = m_scratch.c_str();
	const char *slash = strrchr(url, '/');
	const char *sourceName = slash && slash[1] ? slash + 1 : "download.bin";
	char nameBuf[256];
	size_t nameLen = strcspn(sourceName, "?#");
	if (nameLen == 0) { sourceName = "download.bin"; nameLen = strlen(sourceName); }
	if (nameLen >= sizeof(nameBuf)) nameLen = sizeof(nameBuf) - 1;
	memcpy(nameBuf, sourceName, nameLen);
	nameBuf[nameLen] = 0;
	for (size_t i = 0; i < nameLen; i++)
		if (nameBuf[i] == ':' || nameBuf[i] == '*' || nameBuf[i] == '"' || nameBuf[i] == '<' || nameBuf[i] == '>' || nameBuf[i] == '|') nameBuf[i] = '_';
	paf::string name(nameBuf);
	sceIoMkdir("ux0:download", 0777);
	paf::string destination = paf::string("ux0:download/") + name;
	SceIoStat stat;
	for (int n = 1; sceIoGetstat(destination.c_str(), &stat) >= 0; n++)
	{
		const char *dot = strrchr(name.c_str(), '.');
		paf::string stem = dot ? paf::string(name.c_str(), dot - name.c_str()) : name;
		destination = common::FormatString("ux0:download/%s (%d)%s", stem.c_str(), n, dot ? dot : "");
	}
	int network = ftp::EnsureNetwork();
	if (network < 0)
	{
		dialog::Close();
		dialog::OpenError(g_appPlugin, network);
		return;
	}
	sceSysmoduleLoadModule(SCE_SYSMODULE_HTTPS);
	sceSslInit(300 * 1024);
	sceHttpInit(40 * 1024);
	sceHttpsDisableOption(SCE_HTTPS_FLAG_SERVER_VERIFY);
	int result = downloadUrl(url, destination.c_str(), OnProgress);
	sceHttpTerm();
	sceSslTerm();
	dialog::Close();
	if (result < 0) dialog::OpenError(g_appPlugin, result);
	else dialog::OpenOk(g_appPlugin, g_appPlugin->GetString(msg_qr_result), ToWide(destination).c_str());
}

void BrowserMenu::ReloadRoot()
{
	BrowserPage *rootPage = m_pages.at(0);
	Enqueue(new LoadJob(this, rootPage, true));
}

void BrowserMenu::OnDialog(int32_t buttonCode)
{
	PendingDialog pending = m_pending;

	switch (pending)
	{
	case Pending_ConfirmInstall:
		m_pending = Pending_None;
		if (buttonCode == dialog::ButtonCode_Yes)
		{
			StartOperation(Op_Install);
		}
		break;

	case Pending_ConfirmDelete:
		m_pending = Pending_None;
		if (buttonCode == dialog::ButtonCode_Yes)
		{
			StartOperation(Op_Delete);
		}
		break;

	case Pending_QrOpen:
		m_pending = Pending_None;
		if (buttonCode == dialog::ButtonCode_Yes && !m_scratch.empty())
		{
			if (m_qrDownload)
			{
				dialog::OpenProgress(g_appPlugin, NULL, ToWide(paf::string("Baixando arquivo..." )).c_str());
				Enqueue(new CallJob(this, &BrowserMenu::RunQrDownload));
			}
			else sceAppMgrLaunchAppByUri(0xFFFFF, m_scratch.c_str());
		}
		break;

	case Pending_Operation:
		// The dialog's only button is Cancel; the worker stops at the next block.
		s_cancel = true;
		break;

	default:
		m_pending = Pending_None;
		break;
	}
}
