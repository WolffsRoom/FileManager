#include <sdk_compat.h>
#include <paf.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <psp2/appmgr.h>
#include <psp2/musicexport.h>
#include <psp2/sysmodule.h>
#include <psp2/touch.h>

#include "common.h"
#include "settings.h"
#include "music_menu.h"
#include "core/music.h"
#include "dialog.h"

using namespace paf;

namespace {
	MusicMenu *s_minimizedPlayer = NULL;

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
	m_parentRoot(parentRoot), m_paths(paths), m_index(index), m_seconds(0), m_frame(0), m_playing(false), m_closing(false), m_barTouch(false), m_barTouchX(0), m_minimized(false)
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
	m_seekMarker = m_root->FindChild(plane_seek_marker_page_music);
	m_seekMarker->Hide(common::transition::Type_Reset);
	m_playButton = m_root->FindChild(button_play_page_music);
	m_playGlyph = m_root->FindChild(glyph_play_page_music);
	m_pauseGlyph = m_root->FindChild(glyph_pause_page_music);

	m_playGlyph->Hide(common::transition::Type_Reset);
	SetText(m_root->FindChild(button_export_page_music), Utf8(msg_music_export).c_str());
	SetText(m_root->FindChild(button_background_page_music), Utf8(msg_music_background).c_str());
	SetText(m_root->FindChild(button_native_music_page_music), Utf8(msg_music_open_native).c_str());

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
	AddKey(m_root->FindChild(button_export_page_music), 0,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<MusicMenu *>(userdata)->ExportToMusic();
	}, this);
	AddKey(m_root->FindChild(button_background_page_music), 0,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<MusicMenu *>(userdata)->ListenInBackground();
	}, this);
	AddKey(m_root->FindChild(button_native_music_page_music), 0,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<MusicMenu *>(userdata)->OpenInMusic();
	}, this);

	Play(index);
	common::MainThreadCallList::Register(UpdateTask, this);
}

MusicMenu::~MusicMenu()
{
	if (s_minimizedPlayer == this) s_minimizedPlayer = NULL;
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
	if (s_minimizedPlayer == this) s_minimizedPlayer = NULL;
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
	m_playGlyph->Hide(common::transition::Type_Reset);
	m_pauseGlyph->Show(common::transition::Type_Reset);
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
	bool playing = musicIsPlaying();
	if (playing) { m_playGlyph->Hide(common::transition::Type_Reset); m_pauseGlyph->Show(common::transition::Type_Reset); }
	else { m_pauseGlyph->Hide(common::transition::Type_Reset); m_playGlyph->Show(common::transition::Type_Reset); }
}

int32_t MusicMenu::ExportCurrent(bool notify)
{
	const char *path = m_paths.at(m_index).c_str();
	int32_t res = sceSysmoduleLoadModule(SCE_SYSMODULE_MUSIC_EXPORT);
	if (res >= 0)
	{
		void *work = malloc(64 * 1024);
		if (work)
		{
			MusicExportParam param;
			char out[1024];
			memset(&param, 0, sizeof(param));
			memset(out, 0, sizeof(out));
			res = sceMusicExportFromFile(path, &param, work, NULL, NULL, NULL, out, sizeof(out));
			// The native library reports this when the song is already present.
			if ((uint32_t)res == 0x8010530A)
				res = 0;
			free(work);
		}
	}
	if (res < 0)
		dialog::OpenError(g_appPlugin, res);
	else if (notify)
		dialog::OpenOk(g_appPlugin, g_appPlugin->GetString(msg_music_export), g_appPlugin->GetString(msg_music_export_done));
	return res;
}

void MusicMenu::ExportToMusic()
{
	ExportCurrent(true);
}

void MusicMenu::OpenInMusic()
{
	if (ExportCurrent(false) >= 0)
		sceAppMgrLaunchAppByName(0x60000, "NPXS10009", NULL);
}

void MusicMenu::ListenInBackground()
{
	if (m_closing || m_minimized) return;
	m_minimized = true;
	s_minimizedPlayer = this;
	m_root->SetActivate(false);
	m_root->Hide(common::transition::Type_FadeinFast);
	m_parentRoot->SetActivate(true);
}

bool MusicMenu::HasMinimized()
{
	return s_minimizedPlayer != NULL && musicIsPlaying();
}

void MusicMenu::RestoreMinimized()
{
	if (!s_minimizedPlayer) return;
	MusicMenu *menu = s_minimizedPlayer;
	s_minimizedPlayer = NULL;
	menu->m_minimized = false;
	menu->m_parentRoot->SetActivate(false);
	menu->m_root->Show(common::transition::Type_FadeinFast);
	menu->m_root->SetActivate(true);
}

void MusicMenu::Update()
{
	SceTouchData touch;
	memset(&touch, 0, sizeof(touch));
	sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
	if (touch.reportNum)
	{
		float x = touch.report[0].x * (960.0f / 1920.0f);
		float y = touch.report[0].y * (544.0f / 1088.0f);
		bool overTrack = x >= 420.0f && x <= 920.0f && y >= 252.0f && y <= 330.0f;
		if (overTrack || m_barTouch)
		{
			m_barTouch = true;
			m_barTouchX = x;
			float pct = (m_barTouchX - 440.0f) / k_barWidth;
			if (pct < 0.0f) pct = 0.0f;
			if (pct > 1.0f) pct = 1.0f;
			UpdateSeekPreview(pct);
		}
	}
	else if (m_barTouch)
	{
		// Use the last touch position retained by the touch service.
		float pct = (m_barTouchX - 440.0f) / k_barWidth;
		if (pct < 0.0f) pct = 0.0f;
		if (pct > 1.0f) pct = 1.0f;
		musicSeek(pct * m_seconds);
		m_barTouch = false;
		m_seekMarker->Hide(common::transition::Type_FadeinFast);
	}
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
			m_pauseGlyph->Hide(common::transition::Type_Reset);
			m_playGlyph->Show(common::transition::Type_Reset);
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

void MusicMenu::UpdateSeekPreview(float percent)
{
	if (m_seconds <= 0) return;
	long target = (long)(percent * m_seconds);
	char text[16];
	snprintf(text, sizeof(text), "%02ld:%02ld", target / 60, target % 60);
	SetText(m_elapsed, text);
	m_barFill->SetSize(k_barWidth * percent, 8.0f, 0.0f);
	m_seekMarker->SetPos(k_barWidth * percent - k_barWidth * 0.5f, 0.0f, 0.0f);
	m_seekMarker->Show(common::transition::Type_FadeinFast);
}
