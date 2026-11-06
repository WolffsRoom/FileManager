#include <sdk_compat.h>
#include <paf.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "settings.h"
#include "text_view.h"

using namespace paf;

namespace {

	const int32_t k_maxFile = 2 * 1024 * 1024;  // larger files show their first 2 MB
	const int32_t k_wrapColumns = 76;           // characters per row at 22 px

	// Start of the next UTF-8 character, so wrapping never cuts one in half.
	inline bool IsContinuation(char c)
	{
		return (c & 0xC0) == 0x80;
	}
}

TextView::TextView(ui::Widget *parentRoot, const char *path) : m_parentRoot(parentRoot), m_data(NULL)
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
		reinterpret_cast<TextView *>(userdata)->Close();
	}, this);

	// Read and split into rows: newlines, then wrap long lines.
	int32_t size = 0;
	SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd >= 0)
	{
		size = sceIoLseek32(fd, 0, SCE_SEEK_END);
		if (size > k_maxFile)
		{
			size = k_maxFile;
		}
		sceIoLseek32(fd, 0, SCE_SEEK_SET);
		m_data = (char *)malloc(size + 1);
		if (m_data)
		{
			size = sceIoRead(fd, m_data, size);
			if (size < 0)
			{
				size = 0;
			}
			m_data[size] = 0;
		}
		sceIoClose(fd);
	}

	if (m_data)
	{
		int32_t i = 0;
		// UTF-8 byte order mark
		if (size >= 3 && (uint8_t)m_data[0] == 0xEF && (uint8_t)m_data[1] == 0xBB && (uint8_t)m_data[2] == 0xBF)
		{
			i = 3;
		}
		int32_t start = i, cols = 0;
		m_lines.push_back(start);
		for (; i < size; i++)
		{
			char c = m_data[i];
			if (c == '\r')
			{
				m_data[i] = 0;
				continue;
			}
			if (c == '\n')
			{
				m_data[i] = 0;
				m_lines.push_back(i + 1);
				cols = 0;
				continue;
			}
			if (c == '\t')
			{
				m_data[i] = ' ';
			}
			else if (c == 0)
			{
				m_data[i] = '.';  // binary content stays readable
			}
			if (!IsContinuation(c) && ++cols > k_wrapColumns)
			{
				// Wrapping needs a terminator: shift is impossible in place, so
				// rows are tracked by start offset and drawn up to the next row.
				m_lines.push_back(i);
				cols = 1;
			}
		}
	}

	m_list = static_cast<ui::ListView *>(m_root->FindChild(list_view_page_text_view));
	m_list->SetItemFactory(new ListViewCb(this));
	m_list->InsertSegment(0, 1);
	math::v4 cellSize(960.0f, 32.0f);
	m_list->SetCellSizeDefault(0, cellSize);
	m_list->SetSegmentLayoutType(0, ui::ListView::LAYOUT_TYPE_LIST);
	if (!m_lines.empty())
	{
		m_list->InsertCell(0, 0, m_lines.size());
	}
}

TextView::~TextView()
{
	m_list->SetName(static_cast<uint32_t>(0));
	Plugin::PageCloseParam closeParam;
	closeParam.fade = true;
	closeParam.fade_time_ms = 200.0f;
	g_appPlugin->PageClose(page_text_view, closeParam);
	free(m_data);
	m_parentRoot->SetActivate(true);
}

void TextView::Close()
{
	m_root->SetActivate(false);
	DeferDelete(this);
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

	// A row runs to the next row's start, or to the terminator written for a newline.
	const uint32_t start = m_lines.at(param.cell_index);
	uint32_t end = param.cell_index + 1 < (int32_t)m_lines.size() ? m_lines.at(param.cell_index + 1) : start + strlen(m_data + start);
	const char *nul = (const char *)memchr(m_data + start, 0, end - start);
	if (nul)
	{
		end = nul - m_data;
	}
	paf::string line(m_data + start, end - start);
	paf::wstring wide;
	common::Utf8ToUtf16(line, &wide);
	item->FindChild(button_text_line)->SetString(wide);

	return static_cast<ui::ListItem *>(item);
}
