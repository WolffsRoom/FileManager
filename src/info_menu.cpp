#include <sdk_compat.h>
#include <paf.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "format.h"
#include "info_menu.h"
#include "core/bubble.h"
#include "core/install.h"

using namespace paf;

namespace {

	const int64_t k_maxPreviewFile = 12 * 1024 * 1024;

	paf::string Utf8(uint32_t id)
	{
		paf::string s;
		common::Utf16ToUtf8(paf::wstring(g_appPlugin->GetString(id)), &s);
		return s;
	}

	void SetText(ui::Widget *w, const char *utf8)
	{
		paf::wstring text;
		common::Utf8ToUtf16(utf8, &text);
		w->SetString(text);
	}

	intrusive_ptr<graph::Surface> LoadPng(void *data, uint32_t size)
	{
		int32_t res = -1;
		common::SharedPtr<MemFile> file = MemFile::Open(data, size, &res);
		if (res != SCE_PAF_OK)
		{
			return intrusive_ptr<graph::Surface>();
		}
		return graph::Surface::Load(gutil::GetDefaultSurfacePool(), (common::SharedPtr<File> &)file);
	}
}

InfoMenu::InfoMenu(ui::Widget *parentRoot, Mode mode, const paf::string &path, const fs::Entry *entry, ResultCallback cb, void *userdata) :
	m_parentRoot(parentRoot), m_cb(cb), m_userdata(userdata), m_accepted(false)
{
	Plugin::PageOpenParam openParam;
	openParam.graphics_flag = 0x80;
	openParam.overwrite_draw_priority = 8;
	m_root = g_appPlugin->PageOpen(page_info, openParam);
	m_parentRoot->SetActivate(false);

	m_picture = m_root->FindChild(plane_picture_page_info);
	graph::PlaneObj *po = static_cast<graph::PlaneObj *>(m_picture->GetDrawObj(ui::Plane::OBJ_PLANE));
	po->SetScaleMode(graph::PlaneObj::SCALE_ASPECT_SIZE, graph::PlaneObj::SCALE_ASPECT_SIZE);

	const char *slash = strrchr(path.c_str(), '/');
	const char *name = slash ? slash + 1 : path.c_str();
	paf::string title = name;
	paf::string sizeText = format::Size(entry->size);

	// Picture and details by kind
	bool havePicture = false;
	if (entry->kind == fs::Kind_Vpk)
	{
		VpkInfo info;
		memset(&info, 0, sizeof(info));
		if (readVpkInfo(path.c_str(), &info) >= 0)
		{
			if (info.title[0])
			{
				title = info.title;
			}
			AddLine(common::FormatString("%s: %s", Utf8(msg_info_version).c_str(), info.version[0] ? info.version : "-").c_str());
			AddLine(common::FormatString("%s: %s", Utf8(msg_info_titleid).c_str(), info.titleid).c_str());
			if (info.icon)
			{
				void *bubble = NULL;
				uint32_t bubbleSize = 0;
				if (makeBubblePng(info.icon, info.iconSize, &bubble, &bubbleSize) >= 0)
				{
					ShowPicture(bubble, bubbleSize);
					free(bubble);
					havePicture = true;
				}
				free(info.icon);
			}
		}
		AddLine(common::FormatString("%s: %s", Utf8(msg_info_size).c_str(), sizeText.c_str()).c_str());
	}
	else if (entry->kind == fs::Kind_Image && (int64_t)entry->size <= k_maxPreviewFile)
	{
		int32_t res = -1;
		auto file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &res);
		if (res == 0)
		{
			intrusive_ptr<graph::Surface> tex = graph::Surface::Load(gutil::GetDefaultSurfacePool(), (common::SharedPtr<File> &)file);
			if (tex.get())
			{
				m_picture->SetTexture(tex, 0, 0);
				havePicture = true;
				AddLine(common::FormatString("%s: %d x %d", Utf8(msg_info_dimensions).c_str(), tex->GetWidth(), tex->GetHeight()).c_str());
			}
		}
		AddLine(common::FormatString("%s: %s", Utf8(msg_info_size).c_str(), sizeText.c_str()).c_str());
		AddLine(path.c_str());
	}
	else
	{
		AddLine(common::FormatString("%s: %s", Utf8(msg_info_size).c_str(), sizeText.c_str()).c_str());
		AddLine(path.c_str());
	}

	if (!havePicture)
	{
		// the file's own icon from the list
		m_picture->SetTexture(g_appPlugin->GetTexture(entry->kind == fs::Kind_Vpk ? tex_ui_vpk : tex_ui_file), 0, 0);
	}

	if (mode == Mode_Installed)
	{
		// "Installed "<name>". It is now on the home screen."
		paf::string done = common::FormatString(Utf8(msg_install_done_text).c_str(), title.c_str());
		m_text = done;
		title = Utf8(msg_install_done_title);
	}
	SetText(m_root->FindChild(text_title_page_info), title.c_str());
	SetText(m_root->FindChild(text_details_page_info), m_text.c_str());

	ui::Widget *question = m_root->FindChild(text_question_page_info);
	ui::Widget *ok = m_root->FindChild(button_ok_page_info);
	ui::Widget *cancel = m_root->FindChild(button_cancel_page_info);
	if (mode == Mode_Installed)
	{
		question->Hide(common::transition::Type_Reset);
		SetText(ok, Utf8(msg_install_run).c_str());
		SetText(cancel, Utf8(msg_exit).c_str());
		cancel->SetPos(-140.0f, -160.0f, 0.0f);
		ok->SetPos(140.0f, -160.0f, 0.0f);
		cancel->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE);
	}
	else if (mode == Mode_Install)
	{
		SetText(question, Utf8(msg_install_question).c_str());
		SetText(ok, Utf8(msg_install_button).c_str());
		SetText(cancel, Utf8(msg_cancel).c_str());
		cancel->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE);
	}
	else
	{
		question->Hide(common::transition::Type_Reset);
		cancel->Hide(common::transition::Type_Reset);
		SetText(ok, Utf8(msg_ok).c_str());
		ok->SetPos(0.0f, -150.0f, 0.0f);
		ok->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE);
	}

	ok->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<InfoMenu *>(userdata)->Close(true);
	}, this);
	cancel->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<InfoMenu *>(userdata)->Close(false);
	}, this);
}

InfoMenu::~InfoMenu()
{
	Plugin::PageCloseParam closeParam;
	closeParam.fade = true;
	closeParam.fade_time_ms = 200.0f;
	g_appPlugin->PageClose(page_info, closeParam);
	m_parentRoot->SetActivate(true);
}

void InfoMenu::AddLine(const char *text)
{
	if (!m_text.empty())
	{
		m_text += "\n";
	}
	m_text += text;
}

void InfoMenu::ShowPicture(void *png, uint32_t size)
{
	intrusive_ptr<graph::Surface> tex = LoadPng(png, size);
	if (tex.get())
	{
		m_picture->SetTexture(tex, 0, 0);
		m_picture->SetScale(0.18f, 0.18f, 1.0f);
		m_picture->SetScale(1.0f, 1.0f, 1.0f, new Timer(0.34f, Timer::FUNC_SINE_OUT));
	}
}

void InfoMenu::Close(bool accepted)
{
	m_root->SetActivate(false);
	m_accepted = accepted;
	DeferCall([](void *p)
	{
		InfoMenu *menu = static_cast<InfoMenu *>(p);
		ResultCallback cb = menu->m_cb;
		void *userdata = menu->m_userdata;
		bool accepted = menu->m_accepted;
		delete menu;
		if (cb)
		{
			cb(accepted, userdata);
		}
	}, this);
}
