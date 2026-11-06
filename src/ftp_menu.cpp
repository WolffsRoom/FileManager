#include <sdk_compat.h>
#include <paf.h>

#include "common.h"
#include "format.h"
#include "ftp.h"
#include "ftp_menu.h"

using namespace paf;

namespace {

	const float k_barWidth = 560.0f;

	void UpdateTask(void *userdata)
	{
		static_cast<FtpMenu *>(userdata)->Update();
	}

	paf::wstring Wide(const paf::string &s)
	{
		paf::wstring out;
		common::Utf8ToUtf16(s, &out);
		return out;
	}

	// The string table holds patterns with %s; fill them in UTF-8.
	paf::wstring Format(uint32_t pattern, const char *a, const char *b = "")
	{
		paf::string p;
		common::Utf16ToUtf8(paf::wstring(g_appPlugin->GetString(pattern)), &p);
		return Wide(common::FormatString(p.c_str(), a, b));
	}

	void SetText(ui::Widget *w, const paf::wstring &text)
	{
		paf::wstring copy = text;
		w->SetString(copy);
	}
}

FtpMenu::FtpMenu(ui::Widget *parentRoot, CloseCallback cb, void *userdata) :
	m_parentRoot(parentRoot), m_cb(cb), m_userdata(userdata), m_stopped(false), m_frame(0), m_lastActivity(-1), m_lastDone((uint64_t)-1)
{
	Plugin::PageOpenParam openParam;
	openParam.graphics_flag = 0x80;
	openParam.overwrite_draw_priority = 8;
	m_root = g_appPlugin->PageOpen(page_ftp, openParam);
	m_parentRoot->SetActivate(false);

	SetText(m_root->FindChild(text_title_page_ftp), g_appPlugin->GetString(msg_ftp_server));
	m_address = m_root->FindChild(text_address_page_ftp);
	m_connection = m_root->FindChild(text_connection_page_ftp);
	m_activity = m_root->FindChild(text_activity_page_ftp);
	m_progressText = m_root->FindChild(text_progress_page_ftp);
	m_bar = m_root->FindChild(plane_bar_page_ftp);
	m_barFill = m_bar->FindChild(plane_bar_fill_page_ftp);

	ftp::Status st;
	ftp::GetStatus(&st);
	SetText(m_address, Format(msg_ftp_address, st.ip, common::FormatString("%u", st.port).c_str()));

	ui::Widget *ok = m_root->FindChild(button_ok_page_ftp);
	SetText(ok, g_appPlugin->GetString(msg_ok));
	ok->SetKeycode(inputdevice::pad::Data::PAD_ESCAPE);
	ok->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<FtpMenu *>(userdata)->Close(false);
	}, this);

	ui::Widget *cancel = m_root->FindChild(button_cancel_page_ftp);
	SetText(cancel, g_appPlugin->GetString(msg_cancel));
	cancel->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<FtpMenu *>(userdata)->Close(true);
	}, this);

	Update();
	common::MainThreadCallList::Register(UpdateTask, this);
}

FtpMenu::~FtpMenu()
{
	Plugin::PageCloseParam closeParam;
	closeParam.fade = true;
	closeParam.fade_time_ms = 200.0f;
	g_appPlugin->PageClose(page_ftp, closeParam);
	m_parentRoot->SetActivate(true);
}

void FtpMenu::Close(bool stop)
{
	common::MainThreadCallList::Unregister(UpdateTask, this);
	m_root->SetActivate(false);
	m_stopped = stop;
	if (stop)
	{
		ftp::Stop();
	}
	DeferCall([](void *p)
	{
		FtpMenu *menu = static_cast<FtpMenu *>(p);
		CloseCallback cb = menu->m_cb;
		void *userdata = menu->m_userdata;
		bool stopped = menu->m_stopped;
		delete menu;
		cb(stopped, userdata);
	}, this);
}

void FtpMenu::Update()
{
	// Text layout is not free: refresh a few times per second, and only on change.
	if (m_frame++ % 10 != 0)
	{
		return;
	}

	ftp::Status st;
	ftp::GetStatus(&st);
	if (st.activity == m_lastActivity && st.done == m_lastDone)
	{
		return;
	}
	m_lastActivity = st.activity;
	m_lastDone = st.done;

	if (st.activity == ftp::Activity_Waiting)
	{
		SetText(m_connection, g_appPlugin->GetString(msg_ftp_waiting));
	}
	else
	{
		SetText(m_connection, Format(msg_ftp_client, st.client[0] ? st.client : "?"));
	}

	static const uint32_t k_activity[] = {
		msg_ftp_running, msg_ftp_running, msg_ftp_sending, msg_ftp_receiving, msg_ftp_deleting,
		msg_ftp_listing, msg_ftp_folder, msg_ftp_mkdir, msg_ftp_rmdir, msg_ftp_rename,
		msg_ftp_sent, msg_ftp_received_ok, msg_ftp_deleted
	};
	SetText(m_activity, Format(k_activity[st.activity], st.name[0] ? st.name : "/"));

	const bool transfer = st.activity == ftp::Activity_Sending || st.activity == ftp::Activity_Receiving ||
		st.activity == ftp::Activity_Sent || st.activity == ftp::Activity_Received;
	if (!transfer)
	{
		m_bar->Hide(common::transition::Type_Reset);
		SetText(m_progressText, paf::wstring());
		return;
	}

	m_bar->Show(common::transition::Type_Reset);
	paf::string done = format::Size(st.done);
	if (st.total)
	{
		paf::string total = format::Size(st.total);
		uint32_t pct = (uint32_t)(st.done * 100 / st.total);
		float w = k_barWidth * (float)pct / 100.0f;
		m_barFill->SetSize(w, 10.0f, 0.0f);
		paf::wstring text = Format(msg_ftp_progress_of, done.c_str(), total.c_str());
		text += Wide(common::FormatString("  (%u%%)", pct));
		SetText(m_progressText, text);
	}
	else if (st.activity == ftp::Activity_Received)
	{
		m_barFill->SetSize(k_barWidth, 10.0f, 0.0f);
		SetText(m_progressText, Wide(done));
	}
	else
	{
		// Uploads: the client does not say how big the file is, only bytes so far.
		m_barFill->SetSize(k_barWidth, 10.0f, 0.0f);
		SetText(m_progressText, Format(msg_ftp_received, done.c_str()));
	}
}
