#include <sdk_compat.h>
#include <kernel.h>
#include <paf.h>
#include <string.h>
#include <strings.h>

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
#include "core/install.h"

// Second option menu ("More..."): its own page, since the first is still fading out.
#define page_option_menu_sub (0xcd2afa0du)

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
		const char *dot = strrchr(entry->name.c_str(), '.');
		return entry->kind == fs::Kind_Video && dot && strcasecmp(dot, ".mp4") == 0;
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
	m_iconCount(0)
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

	// Start opens the settings; L and R jump to the first and last item.
	ui::Widget *startButton = m_root->FindChild(button_start_page_browser);
	startButton->SetKeycode(inputdevice::pad::Data::PAD_START);
	startButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		BrowserMenu *menu = reinterpret_cast<BrowserMenu *>(userdata);
		if (!dialog::IsActive())
		{
			menu->DoActionPublic(BrowserMenu::Action_Settings);
		}
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

	// Search box of the main folders: filters the list on Enter.
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
		if (e->GetValue(0) == OptionMenu::OptionMenuEvent_Button)
		{
			reinterpret_cast<BrowserMenu *>(userdata)->OnOption(e->GetValue(1));
		}
	}, this);

	PushPage(paf::string());
}

BrowserMenu::~BrowserMenu()
{
	while (PopPage())
	{
	}
	g_appPlugin->PageClose(page_browser, Plugin::PageCloseParam());
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
	else if (fs::IsRootStyle(page->path))
	{
		title = g_appPlugin->GetString(msg_system_partitions);
	}
	else
	{
		common::Utf8ToUtf16(page->path, &title);
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

	// The folders opened from the root page get a search box above the list.
	const bool search = m_pages.size() == 2 && !fs::IsRootStyle(page->path);
	if (search)
	{
		m_search->Show(common::transition::Type_Reset);
		page->list->SetSize(960.0f, 392.0f, 0.0f);
	}
	else
	{
		m_search->Hide(common::transition::Type_Reset);
	}
}

void BrowserMenu::PushPage(const paf::string &path)
{
	Plugin::TemplateOpenParam tmpParam;

	BrowserPage *page = new BrowserPage();
	page->path = path;
	page->cellCount = 0;
	page->focused = -1;
	page->loaded = false;

	g_appPlugin->TemplateOpen(m_browserRoot, template_list_view_generic, tmpParam);
	page->list = static_cast<ui::ListView *>(m_browserRoot->GetChild(m_browserRoot->GetChildrenNum() - 1));
	page->list->SetItemFactory(new ListViewCb(this));
	page->list->InsertSegment(0, 1);
	math::v4 cellSize(960.0f, fs::IsRootStyle(path) ? k_rowRoot : k_rowFile);
	page->list->SetCellSizeDefault(0, cellSize);
	page->list->SetSegmentLayoutType(0, ui::ListView::LAYOUT_TYPE_LIST);

	if (!m_pages.empty())
	{
		if (m_pages.size() > 1)
		{
			BrowserPage *older = m_pages.rbegin()[1];
			older->list->Hide(common::transition::Type_Reset);
			older->list->SetTransitionComplete(false);
		}
		BrowserPage *current = m_pages.back();
		current->list->Show(common::transition::Type_3D_SlideToBack1);
		current->list->SetTransitionComplete(false);
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
		common::transition::DoReverse(-100.0f, page->list, common::transition::Type_3D_SlideFromFront, true, false);
	}

	fs::FreeList(&page->all);
	page->entries.clear();
	delete page;
	m_pages.pop_back();

	if (!isLast)
	{
		BrowserPage *current = m_pages.back();
		current->list->Hide(common::transition::Type_3D_SlideToBack1);
		current->list->Show(common::transition::Type_Reset);
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
	}
	else
	{
		check->Hide(common::transition::Type_Reset);
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

	// Storage bars take the accent color when their cells are built.
	if (!m_pages.empty())
	{
		BrowserPage *rootPage = m_pages.at(0);
		if (rootPage->cellCount > 0 && rootPage->loaded)
		{
			rootPage->list->DeleteCell(0, 0, rootPage->cellCount);
			rootPage->list->InsertCell(0, 0, rootPage->cellCount);
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

	intrusive_ptr<graph::Surface> tex = Icon(IconFor(entry));
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
	else if (Previewable(entry))
	{
		OpenPreview(index);
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
		}
		else
		{
			check->Hide(common::transition::Type_Reset);
		}
	}
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
		m_optionActions.push_back(Action_Settings);
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
		m_optionActions.push_back(Action_Settings);
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
		msg_opt_more, msg_opt_add_shortcut, msg_opt_remove_shortcut, msg_ftp_server, msg_qr_code
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
		dialog::OpenYesNo(g_appPlugin, g_appPlugin->GetString(msg_confirm_delete_title), g_appPlugin->GetString(msg_confirm_delete));
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
		dialog::OpenOk(g_appPlugin, g_appPlugin->GetString(msg_qr_code), g_appPlugin->GetString(msg_qr_unavailable));
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
		ret = sceIoRename(fs::Join(page->path, m_inputTarget).c_str(), fs::Join(page->path, name).c_str());
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

	dialog::OpenProgress(g_appPlugin, NULL, g_appPlugin->GetString(title));
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

	thread::RMutex::MainThreadMutex()->Lock();
	if (result < 0)
	{
		dialog::OpenError(g_appPlugin, result);
	}
	else if (result > 0 && m_opType == Op_Install)
	{
		common::Utf16ToUtf8(paf::wstring(g_appPlugin->GetString(msg_install_done)), &m_scratch);
		paf::wstring text = ToWide(common::FormatString(m_scratch.c_str(), m_installedTitle.c_str()));
		m_pending = Pending_Message;
		dialog::OpenOk(g_appPlugin, g_appPlugin->GetString(msg_install_title), text.c_str());
	}
	Refresh();
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
		dialog::OpenError(g_appPlugin, ret, g_appPlugin->GetString(msg_ftp_no_network));
		return;
	}
	new FtpMenu(m_root, OnFtpMenuClosed, this);
}

void BrowserMenu::OnFtpClosed(bool stopped)
{
	// Files may have changed over FTP; the root also shows whether the server runs.
	Refresh();
	if (m_pages.size() > 1)
	{
		ReloadRoot();
	}
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

	case Pending_Operation:
		// The dialog's only button is Cancel; the worker stops at the next block.
		s_cancel = true;
		break;

	default:
		m_pending = Pending_None;
		break;
	}
}
