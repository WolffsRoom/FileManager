#include <sdk_compat.h>
#include <paf.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "dialog.h"
#include "settings.h"
#include "option_menu.h"
#include "text_input.h"
#include "notice_menu.h"
#include "text_view.h"

using namespace paf;

namespace {

	const int32_t k_maxFile = 2 * 1024 * 1024;  // bigger files open read-only, first 2 MB

	enum Option { Option_Edit, Option_Insert, Option_Delete, Option_Save };

	paf::wstring Str(uint32_t id)
	{
		paf::string s;
		common::Utf16ToUtf8(paf::wstring(g_appPlugin->GetString(id)), &s);
		paf::wstring w;
		common::Utf8ToUtf16(s, &w);
		return w;
	}

	void OnLineDone(const paf::wstring &text, void *userdata)
	{
		reinterpret_cast<TextView *>(userdata)->OnLineEntered(text);
	}
}

TextView::TextView(ui::Widget *parentRoot, const char *path) :
	m_parentRoot(parentRoot), m_path(path), m_rows(0), m_focused(-1), m_dirty(false), m_crlf(false), m_truncated(false), m_edit(Edit_Change)
{
	Plugin::PageOpenParam openParam;
	openParam.graphics_flag = 0x80;
	openParam.overwrite_draw_priority = 8;
	m_root = g_appPlugin->PageOpen(page_text_view, openParam);
	m_parentRoot->SetActivate(false);

	theme::ApplyBackground(m_root->FindChild(plane_base_bg_page_text_view));

	const char *slash = strrchr(path, '/');
	paf::wstring title;
	common::Utf8ToUtf16(slash ? slash + 1 : path, &title);
	m_root->FindChild(text_top)->SetString(title);

	ui::Widget *back = m_root->FindChild(button_back_page_text_view);
	back->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE);
	back->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<TextView *>(userdata)->OnBack();
	}, this);

	ui::Widget *options = m_root->FindChild(button_options_page_text_view);
	options->SetKeycode(inputdevice::pad::Data::PAD_MENU);
	options->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<TextView *>(userdata)->OnOptions();
	}, this);

	m_root->AddEventCallback(OptionMenu::OptionMenuEvent,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		if (e->GetValue(0) == OptionMenu::OptionMenuEvent_Button && e->GetValue(2) == TextView::OptionTag)
		{
			reinterpret_cast<TextView *>(userdata)->OnOption(e->GetValue(1));
		}
	}, this);

	// Read, split into lines; remember CRLF so saving keeps the file's format.
	SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd >= 0)
	{
		int32_t size = sceIoLseek32(fd, 0, SCE_SEEK_END);
		if (size > k_maxFile)
		{
			size = k_maxFile;
			m_truncated = true;
		}
		sceIoLseek32(fd, 0, SCE_SEEK_SET);
		char *data = (char *)malloc(size + 1);
		if (data)
		{
			size = sceIoRead(fd, data, size);
			if (size < 0)
			{
				size = 0;
			}
			data[size] = 0;
			int32_t i = 0;
			if (size >= 3 && (uint8_t)data[0] == 0xEF && (uint8_t)data[1] == 0xBB && (uint8_t)data[2] == 0xBF)
			{
				i = 3;
			}
			int32_t start = i;
			for (; i <= size; i++)
			{
				if (i == size || data[i] == '\n')
				{
					int32_t end = i;
					if (end > start && data[end - 1] == '\r')
					{
						end--;
						m_crlf = true;
					}
					for (int32_t k = start; k < end; k++)
					{
						if (data[k] == '\t')
						{
							data[k] = ' ';
						}
						else if (data[k] == 0)
						{
							data[k] = '.';
						}
					}
					if (i < size || end > start)
					{
						m_lines.push_back(paf::string(data + start, end - start));
					}
					start = i + 1;
				}
			}
			free(data);
		}
		sceIoClose(fd);
	}
	if (m_lines.empty())
	{
		m_lines.push_back(paf::string());
	}

	m_list = static_cast<ui::ListView *>(m_root->FindChild(list_view_page_text_view));
	m_list->SetItemFactory(new ListViewCb(this));
	m_list->InsertSegment(0, 1);
	math::v4 cellSize(960.0f, 32.0f);
	m_list->SetCellSizeDefault(0, cellSize);
	m_list->SetSegmentLayoutType(0, ui::ListView::LAYOUT_TYPE_LIST);
	Rebuild();
}

TextView::~TextView()
{
	m_list->SetName(static_cast<uint32_t>(0));
	Plugin::PageCloseParam closeParam;
	closeParam.fade = true;
	closeParam.fade_time_ms = 200.0f;
	g_appPlugin->PageClose(page_text_view, closeParam);
	m_parentRoot->SetActivate(true);
}

void TextView::Rebuild()
{
	if (m_rows > 0)
	{
		m_list->DeleteCell(0, 0, m_rows);
	}
	m_rows = m_lines.size();
	m_list->InsertCell(0, 0, m_rows);
}

void TextView::RefreshRow(int32_t row)
{
	ui::ListItem *item = m_list->GetListItem(0, row);
	if (item && row >= 0 && row < (int32_t)m_lines.size())
	{
		paf::wstring wide;
		common::Utf8ToUtf16(m_lines.at(row), &wide);
		item->FindChild(button_text_line)->SetString(wide);
	}
}

ui::ListItem *TextView::CreateLine(ui::listview::ItemFactory::CreateParam &param)
{
	if (!param.list_view->GetName().GetIDHash() || param.cell_index < 0 || param.cell_index >= (int32_t)m_lines.size())
	{
		return new ui::ListItem(param.parent, 0);
	}

	Plugin::TemplateOpenParam tmpParam;
	ui::Widget *targetRoot = param.parent;
	g_appPlugin->TemplateOpen(targetRoot, template_text_line, tmpParam);
	ui::Widget *item = targetRoot->GetChild(targetRoot->GetChildrenNum() - 1);

	ui::Widget *button = item->FindChild(button_text_line);
	button->SetName(param.cell_index);
	paf::wstring wide;
	common::Utf8ToUtf16(m_lines.at(param.cell_index), &wide);
	button->SetString(wide);

	button->AddEventCallback(ui::Handler::CB_FOCUS_IN,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<TextView *>(userdata)->m_focused = static_cast<ui::Widget *>(self)->GetName().GetIDHash();
	}, this);
	button->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		TextView *view = reinterpret_cast<TextView *>(userdata);
		view->m_focused = static_cast<ui::Widget *>(self)->GetName().GetIDHash();
		view->OnOption(Option_Edit);
	}, this);

	return static_cast<ui::ListItem *>(item);
}

void TextView::OnOptions()
{
	paf::vector<OptionMenu::Button> buttons;
	static const uint32_t k_labels[] = { msg_text_edit_line, msg_text_insert_line, msg_text_delete_line, msg_text_save };
	for (uint32_t i = 0; i < sizeof(k_labels) / sizeof(k_labels[0]); i++)
	{
		OptionMenu::Button bt;
		bt.label = Str(k_labels[i]);
		buttons.push_back(bt);
	}
	new OptionMenu(g_appPlugin, m_root, &buttons, 0xd098335eu, OptionTag);
}

void TextView::OnOption(int32_t index)
{
	if (m_truncated && index != Option_Save)
	{
		dialog::OpenOk(g_appPlugin, NULL, g_appPlugin->GetString(msg_text_too_big));
		return;
	}
	int32_t row = m_focused >= 0 && m_focused < (int32_t)m_lines.size() ? m_focused : 0;
	switch (index)
	{
	case Option_Edit:
	{
		m_edit = Edit_Change;
		paf::wstring current;
		common::Utf8ToUtf16(m_lines.at(row), &current);
		new TextInput(m_root, g_appPlugin->GetString(msg_text_edit_line), current, OnLineDone, this);
		break;
	}
	case Option_Insert:
		m_edit = Edit_Insert;
		new TextInput(m_root, g_appPlugin->GetString(msg_text_insert_line), paf::wstring(), OnLineDone, this);
		break;
	case Option_Delete:
		if (m_lines.size() > 1)
		{
			m_lines.erase(m_lines.begin() + row);
		}
		else
		{
			m_lines.at(0) = paf::string();
		}
		m_dirty = true;
		Rebuild();
		break;
	case Option_Save:
	{
		if (m_truncated)
		{
			dialog::OpenOk(g_appPlugin, NULL, g_appPlugin->GetString(msg_text_too_big));
			break;
		}
		int32_t ret = Save();
		if (ret < 0)
		{
			dialog::OpenError(g_appPlugin, ret);
		}
		else
		{
			dialog::OpenOk(g_appPlugin, NULL, g_appPlugin->GetString(msg_text_saved));
		}
		break;
	}
	}
}

void TextView::OnLineEntered(const paf::wstring &text)
{
	// An empty answer is also how TextInput reports Cancel; an emptied line is
	// done with Delete line, so keep it simple: nothing changes.
	if (text.empty() && m_edit == Edit_Change)
	{
		return;
	}
	paf::string line;
	common::Utf16ToUtf8(text, &line);
	int32_t row = m_focused >= 0 && m_focused < (int32_t)m_lines.size() ? m_focused : 0;
	if (m_edit == Edit_Change)
	{
		m_lines.at(row) = line;
		RefreshRow(row);
	}
	else
	{
		paf::vector<paf::string> rebuilt;
		for (int32_t i = 0; i < (int32_t)m_lines.size(); i++)
		{
			rebuilt.push_back(m_lines.at(i));
			if (i == row)
			{
				rebuilt.push_back(line);
			}
		}
		m_lines = rebuilt;
		Rebuild();
	}
	m_dirty = true;
}

int32_t TextView::Save()
{
	SceUID fd = sceIoOpen(m_path.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
	if (fd < 0)
	{
		return fd;
	}
	const char *eol = m_crlf ? "\r\n" : "\n";
	for (uint32_t i = 0; i < m_lines.size(); i++)
	{
		const paf::string &l = m_lines.at(i);
		if (l.length())
		{
			int32_t ret = sceIoWrite(fd, l.c_str(), l.length());
			if (ret < 0)
			{
				sceIoClose(fd);
				return ret;
			}
		}
		if (i + 1 < m_lines.size())
		{
			sceIoWrite(fd, eol, strlen(eol));
		}
	}
	sceIoClose(fd);
	m_dirty = false;
	return 0;
}

void TextView::OnSaveAnswer(int32_t choice)
{
	if (choice == 0)
	{
		int32_t ret = Save();
		if (ret < 0)
		{
			dialog::OpenError(g_appPlugin, ret);
			return;
		}
	}
	m_dirty = false;
	OnBack();
}

void TextView::OnBack()
{
	if (m_dirty)
	{
		new NoticeMenu(m_root, msg_text_unsaved_title, msg_text_unsaved, msg_text_save, msg_text_discard,
		[](int32_t choice, void *userdata)
		{
			reinterpret_cast<TextView *>(userdata)->OnSaveAnswer(choice);
		}, this);
		return;
	}
	m_root->SetActivate(false);
	DeferDelete(this);
}
