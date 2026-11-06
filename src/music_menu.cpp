#include <sdk_compat.h>
#include <paf.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "settings.h"
#include "music_menu.h"
#include "core/music.h"

using namespace paf;

namespace {

	const float k_barWidth = 480.0f;

	void UpdateTask(void *userdata)
	{
		static_cast<MusicMenu *>(userdata)->Update();
	}

	void SetText(ui::Widget *w, const char *utf8)
	{
		paf::wstring text;
		common::Utf8ToUtf16(utf8, &text);
		w->SetString(text);
	}

	paf::string Utf8(uint32_t id)
	{
		paf::string s;
		common::Utf16ToUtf8(paf::wstring(g_appPlugin->GetString(id)), &s);
		return s;
	}

	void AddKey(ui::Widget *button, uint32_t key, ui::HandlerCB cb, void *userdata)
	{
		if (key)
		{
			button->SetKeycode(key);
		}
		button->AddEventCallback(ui::Button::CB_BTN_DECIDE, cb, userdata);
	}
}

MusicMenu::MusicMenu(ui::Widget *parentRoot, const paf::vector<paf::string> &paths, int32_t index) :
	m_parentRoot(parentRoot), m_paths(paths), m_index(index), m_seconds(0), m_frame(0), m_playing(false), m_closing(false)
{
	Plugin::PageOpenParam openParam;
	openParam.graphics_flag = 0x80;
	openParam.overwrite_draw_priority = 8;
	m_root = g_appPlugin->PageOpen(page_music, openParam);
	m_parentRoot->SetActivate(false);

	theme::ApplyBackground(m_root->FindChild(plane_base_bg_page_music));
	SetText(m_root->FindChild(text_top), Utf8(msg_music_title).c_str());

	m_cover = m_root->FindChild(plane_cover_page_music);
	graph::PlaneObj *po = static_cast<graph::PlaneObj *>(m_cover->GetDrawObj(ui::Plane::OBJ_PLANE));
	po->SetScaleMode(graph::PlaneObj::SCALE_ASPECT_SIZE, graph::PlaneObj::SCALE_ASPECT_SIZE);
	m_title = m_root->FindChild(text_song_page_music);
	m_artist = m_root->FindChild(text_artist_page_music);
	m_album = m_root->FindChild(text_album_page_music);
	m_elapsed = m_root->FindChild(text_elapsed_page_music);
	m_total = m_root->FindChild(text_total_page_music);
	m_barFill = m_root->FindChild(plane_bar_fill_page_music);
	m_playButton = m_root->FindChild(button_play_page_music);

	SetText(m_root->FindChild(button_prev_page_music), Utf8(msg_music_prev).c_str());
	SetText(m_root->FindChild(button_next_page_music), Utf8(msg_music_next).c_str());

	AddKey(m_root->FindChild(button_back_page_music), inputdevice::pad::Data::PAD_ESCAPE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<MusicMenu *>(userdata)->Close();
	}, this);
	AddKey(m_playButton, inputdevice::pad::Data::PAD_ENTER,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<MusicMenu *>(userdata)->TogglePause();
	}, this);
	AddKey(m_root->FindChild(button_prev_page_music), inputdevice::pad::Data::PAD_L,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<MusicMenu *>(userdata)->Step(-1);
	}, this);
	AddKey(m_root->FindChild(button_next_page_music), inputdevice::pad::Data::PAD_R,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<MusicMenu *>(userdata)->Step(1);
	}, this);
	AddKey(m_root->FindChild(button_rewind_page_music), inputdevice::pad::Data::PAD_LEFT,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<MusicMenu *>(userdata)->Seek(-10);
	}, this);
	AddKey(m_root->FindChild(button_forward_page_music), inputdevice::pad::Data::PAD_RIGHT,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<MusicMenu *>(userdata)->Seek(10);
	}, this);

	Play(index);
	common::MainThreadCallList::Register(UpdateTask, this);
}

MusicMenu::~MusicMenu()
{
	Plugin::PageCloseParam closeParam;
	closeParam.fade = true;
	closeParam.fade_time_ms = 200.0f;
	g_appPlugin->PageClose(page_music, closeParam);
	m_parentRoot->SetActivate(true);
}

void MusicMenu::Close()
{
	if (m_closing)
	{
		return;
	}
	m_closing = true;
	common::MainThreadCallList::Unregister(UpdateTask, this);
	musicClose();
	m_root->SetActivate(false);
	DeferDelete(this);
}

void MusicMenu::ShowCover(const char *path, int32_t type, int32_t offset, int32_t length)
{
	intrusive_ptr<graph::Surface> tex;
	if ((type == 1 || type == 2) && length > 0 && length < 4 * 1024 * 1024)
	{
		SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
		if (fd >= 0)
		{
			void *buf = malloc(length);
			if (buf)
			{
				sceIoLseek32(fd, offset, SCE_SEEK_SET);
				if (sceIoRead(fd, buf, length) == length)
				{
					int32_t res = -1;
					common::SharedPtr<MemFile> file = MemFile::Open(buf, length, &res);
					if (res == SCE_PAF_OK)
					{
						tex = graph::Surface::Load(gutil::GetDefaultSurfacePool(), (common::SharedPtr<File> &)file);
					}
				}
				free(buf);
			}
			sceIoClose(fd);
		}
	}
	if (!tex.get())
	{
		tex = g_appPlugin->GetTexture(tex_ui_music);
	}
	m_cover->SetTexture(tex, 0, 0);
}

void MusicMenu::Play(int32_t index)
{
	m_index = index;
	const char *path = m_paths.at(index).c_str();
	MusicInfo info;
	const char *slash = strrchr(path, '/');
	const char *name = slash ? slash + 1 : path;

	if (musicOpen(path, &info) < 0)
	{
		SetText(m_title, name);
		SetText(m_artist, Utf8(msg_preview_error).c_str());
		SetText(m_album, "");
		m_playing = false;
		ShowCover(path, 0, 0, 0);
		return;
	}

	SetText(m_title, info.title[0] ? info.title : name);
	SetText(m_artist, info.artist[0] ? info.artist : Utf8(msg_music_unknown_artist).c_str());
	SetText(m_album, info.album);
	SetText(m_total, info.length);
	SetText(m_elapsed, "00:00");
	m_seconds = info.seconds;
	m_playing = true;
	m_barFill->SetSize(0.0f, 8.0f, 0.0f);
	SetText(m_playButton, Utf8(msg_music_pause).c_str());
	ShowCover(path, info.coverType, info.coverOffset, info.coverLength);
}

void MusicMenu::Step(int32_t delta)
{
	int32_t n = m_paths.size();
	Play((m_index + n + delta) % n);
}

void MusicMenu::Seek(int32_t seconds)
{
	if (!m_playing || m_seconds <= 0)
	{
		return;
	}
	double now = musicPercent() / 100.0 * (double)m_seconds;
	musicSeek(now + seconds);
}

void MusicMenu::TogglePause()
{
	musicTogglePause();
	SetText(m_playButton, Utf8(musicIsPlaying() ? msg_music_pause : msg_music_play).c_str());
}

void MusicMenu::Update()
{
	if (!m_playing || m_frame++ % 15 != 0)
	{
		return;
	}
	if (musicEnded())
	{
		// next song, and stop after the last one
		if (m_index + 1 < (int32_t)m_paths.size())
		{
			Play(m_index + 1);
		}
		else
		{
			m_playing = false;
			musicClose();
			SetText(m_playButton, Utf8(msg_music_play).c_str());
		}
		return;
	}
	char now[16];
	musicTime(now);
	SetText(m_elapsed, now);
	float pct = musicPercent();
	if (pct < 0.0f) pct = 0.0f;
	if (pct > 100.0f) pct = 100.0f;
	m_barFill->SetSize(k_barWidth * pct / 100.0f, 8.0f, 0.0f);
}
