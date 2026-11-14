#include <sdk_compat.h>
#include <paf.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <psp2/musicexport.h>
#include <psp2/sysmodule.h>

#include "common.h"
#include "settings.h"
#include "music_menu.h"
#include "core/music.h"
#include "dialog.h"
#include "option_menu.h"

using namespace paf;

namespace {
	MusicMenu *s_minimizedPlayer = NULL;

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
	m_parentRoot(parentRoot), m_paths(paths), m_index(index), m_seconds(0), m_frame(0), m_playing(false), m_closing(false), m_seekChanging(false), m_minimized(false), m_minimizePending(false), m_minimizeFrames(0), m_restorePending(false), m_restoreFrames(0), m_waveformVisible(false), m_hasCover(false), m_screenOff(false), m_screenTapTick(0), m_screenTapCount(0)
{
	Plugin::PageOpenParam openParam;
	openParam.graphics_flag = 0x80;
	openParam.overwrite_draw_priority = 8;
	m_root = g_appPlugin->PageOpen(page_music, openParam);
	m_parentRoot->SetActivate(false);

	m_background = m_root->FindChild(plane_base_bg_page_music);
	theme::ApplyBackground(m_background);
	const theme::Info &overlayTheme = theme::At(settings::Get().theme);
	// Music keeps a solid, readable canvas; only a subtle amount of the browser
	// background remains visible regardless of the image-preview preference.
	m_background->SetColor(overlayTheme.bg[0], overlayTheme.bg[1], overlayTheme.bg[2], 0.92f);
	SetText(m_root->FindChild(text_top), Utf8(msg_music_title).c_str());

	m_cover = m_root->FindChild(plane_cover_page_music);
	m_coverShadow = m_root->FindChild(plane_cover_shadow_page_music);
	// The old common glow texture produced a white frame around album covers.
	// Keep the artwork clean, like the native Music application.
	m_coverShadow->Hide(common::transition::Type_Reset);
	const uint32_t sliceIds[GenieSlices] = {
		plane_cover_slice_0_page_music, plane_cover_slice_1_page_music,
		plane_cover_slice_2_page_music, plane_cover_slice_3_page_music,
		plane_cover_slice_4_page_music, plane_cover_slice_5_page_music
	};
	for (int32_t i = 0; i < GenieSlices; ++i)
	{
		m_genie[i] = m_root->FindChild(sliceIds[i]);
		m_genie[i]->Hide(common::transition::Type_Reset);
	}
	graph::PlaneObj *po = static_cast<graph::PlaneObj *>(m_cover->GetDrawObj(ui::Plane::OBJ_PLANE));
	po->SetScaleMode(graph::PlaneObj::SCALE_ASPECT_SIZE, graph::PlaneObj::SCALE_ASPECT_SIZE);
	m_title = m_root->FindChild(text_song_page_music);
	m_artist = m_root->FindChild(text_artist_page_music);
	m_album = m_root->FindChild(text_album_page_music);
	m_elapsed = m_root->FindChild(text_elapsed_page_music);
	m_total = m_root->FindChild(text_total_page_music);
	m_seekSlider = static_cast<ui::ProgressBarTouch *>(m_root->FindChild(progressbar_seek_page_music));
	m_seekSlider->SetMinValue(0.0f);
	m_seekSlider->SetMaxValue(100.0f);
	m_seekSlider->AddEventCallback(ui::ProgressBarTouch::CB_PROGRESSBAR_CHANGED,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata)
	{
		reinterpret_cast<MusicMenu *>(userdata)->OnSeekChanged();
	}, this);
	m_waveform = m_root->FindChild(plane_waveform_page_music);
	const uint32_t waveIds[WaveBars] = {
		plane_wave_00_page_music, plane_wave_01_page_music, plane_wave_02_page_music, plane_wave_03_page_music,
		plane_wave_04_page_music, plane_wave_05_page_music, plane_wave_06_page_music, plane_wave_07_page_music,
		plane_wave_08_page_music, plane_wave_09_page_music, plane_wave_10_page_music, plane_wave_11_page_music,
		plane_wave_12_page_music, plane_wave_13_page_music, plane_wave_14_page_music, plane_wave_15_page_music,
		plane_wave_16_page_music, plane_wave_17_page_music, plane_wave_18_page_music, plane_wave_19_page_music,
		plane_wave_20_page_music, plane_wave_21_page_music, plane_wave_22_page_music, plane_wave_23_page_music,
		plane_wave_24_page_music, plane_wave_25_page_music, plane_wave_26_page_music, plane_wave_27_page_music,
		plane_wave_28_page_music, plane_wave_29_page_music, plane_wave_30_page_music, plane_wave_31_page_music,
		plane_wave_32_page_music, plane_wave_33_page_music, plane_wave_34_page_music, plane_wave_35_page_music,
		plane_wave_36_page_music, plane_wave_37_page_music, plane_wave_38_page_music, plane_wave_39_page_music
	};
	for (int32_t i = 0; i < WaveBars; ++i)
	{
		m_waveBars[i] = m_root->FindChild(waveIds[i]);
		m_waveLevels[i] = 0.0f;
	}
	m_waveform->Hide(common::transition::Type_Reset);
	m_screenOffButton = m_root->FindChild(button_screen_off_page_music);
	m_screenOffButton->Hide(common::transition::Type_Reset);
	m_screenOffButton->SetActivate(false);
	m_screenOffButton->AddEventCallback(ui::Button::CB_BTN_PRESS,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata)
	{
		reinterpret_cast<MusicMenu *>(userdata)->OnScreenTap();
	}, this);
	m_playButton = m_root->FindChild(button_play_page_music);
	m_playGlyph = m_root->FindChild(glyph_play_page_music);
	m_pauseGlyph = m_root->FindChild(glyph_pause_page_music);

	m_playGlyph->Hide(common::transition::Type_Reset);
	ui::Widget *options = m_root->FindChild(button_options_page_music);
	options->SetKeycode(inputdevice::pad::Data::PAD_MENU);
	options->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata)
	{
		reinterpret_cast<MusicMenu *>(userdata)->OnOptions();
	}, this);
	m_root->AddEventCallback(OptionMenu::OptionMenuEvent,
	[](int32_t, ui::Handler *, ui::Event *e, void *userdata)
	{
		if (e->GetValue(0) == OptionMenu::OptionMenuEvent_Button && e->GetValue(2) == MusicMenu::OptionTag)
			reinterpret_cast<MusicMenu *>(userdata)->OnOption(e->GetValue(1));
	}, this);

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
	m_hasCover = tex.get() != NULL;
	if (!m_hasCover)
	{
		// Use the real file-type artwork, not the monochrome corner glyph.
		tex = g_appPlugin->GetTexture(tex_file_icon_music);
	}
	m_cover->SetSize(m_hasCover ? 374.0f : 176.0f, m_hasCover ? 370.0f : 176.0f, 0.0f);
	m_cover->SetTexture(tex, 0, 0);
	m_coverSurface = tex;
	for (int32_t i = 0; i < GenieSlices; ++i)
	{
		m_genie[i]->SetTexture(tex, 0, 0);
		graph::PlaneObj *slice = static_cast<graph::PlaneObj *>(m_genie[i]->GetDrawObj(ui::Plane::OBJ_PLANE));
		if (slice) slice->SetTexRegion(math::v4(0.0f, (float)i / GenieSlices, 1.0f, 1.0f / GenieSlices));
	}
}

void MusicMenu::StartGenie(bool expanding)
{
	static const float sourceY[GenieSlices] = { 160.0f, 98.0f, 36.0f, -26.0f, -88.0f, -150.0f };
	const float miniX = -428.0f;
	const float miniY = 208.0f;
	m_cover->Hide(common::transition::Type_Reset);
	m_coverShadow->Hide(common::transition::Type_Reset);
	for (int32_t i = 0; i < GenieSlices; ++i)
	{
		ui::Widget *slice = m_genie[i];
		const float bend = (float)(i - 2) * 2.6f;
		const float duration = 0.38f + (expanding ? (GenieSlices - 1 - i) : i) * 0.035f;
		if (expanding)
		{
			slice->SetPos(miniX + bend, miniY - i * 1.5f, 0.0f);
			slice->SetScale(0.12f, 0.12f, 1.0f);
			slice->SetColor(1.0f, 1.0f, 1.0f, 0.35f);
			slice->Show(common::transition::Type_Reset);
			slice->SetPos(-260.0f, sourceY[i], 0.0f, new Timer(duration, Timer::FUNC_BACK_OUT));
			slice->SetScale(1.0f, 1.0f, 1.0f, new Timer(duration, Timer::FUNC_BACK_OUT));
			slice->SetColor(1.0f, 1.0f, 1.0f, 1.0f, new Timer(duration * 0.72f, Timer::FUNC_SINE_OUT));
		}
		else
		{
			slice->SetPos(-260.0f, sourceY[i], 0.0f);
			slice->SetScale(1.0f, 1.0f, 1.0f);
			slice->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			slice->Show(common::transition::Type_Reset);
			slice->SetPos(miniX + bend, miniY - i * 1.5f, 0.0f, new Timer(duration, Timer::FUNC_BACK_IN));
			slice->SetScale(0.12f, 0.12f, 1.0f, new Timer(duration, Timer::FUNC_BACK_IN));
			slice->SetColor(1.0f, 1.0f, 1.0f, 0.30f, new Timer(duration * 0.86f, Timer::FUNC_SINE_IN));
		}
	}
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
	m_seekSlider->SetValue(0.0f, false);
	m_playGlyph->Hide(common::transition::Type_Reset);
	m_pauseGlyph->Show(common::transition::Type_Reset);
	ShowCover(path, info.coverType, info.coverOffset, info.coverLength);
	musicSetWaveformEnabled(m_waveformVisible ? 1 : 0);
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

void MusicMenu::ToggleWaveform()
{
	m_waveformVisible = !m_waveformVisible;
	musicSetWaveformEnabled(m_waveformVisible ? 1 : 0);
	if (m_waveformVisible) m_waveform->Show(common::transition::Type_FadeinFast);
	else m_waveform->Hide(common::transition::Type_FadeinFast);
}

void MusicMenu::EnableScreenOff()
{
	if (m_screenOff) return;
	m_screenOff = true;
	m_screenTapCount = 0;
	m_screenTapTick = 0;
	// Fully reset the button's previous hidden/pressed state. PAF otherwise
	// leaves the fullscreen hit target stale after the first wake-up.
	m_screenOffButton->Hide(common::transition::Type_Reset);
	m_screenOffButton->SetColor(1.0f, 1.0f, 1.0f, 0.0f);
	m_screenOffButton->Show(common::transition::Type_Reset);
	m_screenOffButton->SetActivate(true);
	m_screenOffButton->SetColor(1.0f, 1.0f, 1.0f, 1.0f, new Timer(0.35f, Timer::FUNC_SINE_OUT));
}

void MusicMenu::OnScreenTap()
{
	if (!m_screenOff) return;
	const uint64_t now = sceKernelGetProcessTimeWide();
	if (!m_screenTapTick || now - m_screenTapTick > 1200000ULL) m_screenTapCount = 0;
	m_screenTapTick = now;
	if (++m_screenTapCount >= 2) DisableScreenOff();
}

void MusicMenu::DisableScreenOff()
{
	if (!m_screenOff) return;
	m_screenOff = false;
	m_screenOffButton->SetActivate(false);
	m_screenOffButton->Hide(common::transition::Type_Reset);
	m_screenOffButton->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	m_screenTapCount = 0;
	m_screenTapTick = 0;
}

void MusicMenu::ListenInBackground()
{
	if (m_closing || m_minimized || m_minimizePending) return;
	m_minimizePending = true;
	m_minimizeFrames = 38;
	StartGenie(false);
	m_root->SetColor(1.0f, 1.0f, 1.0f, 0.82f, new Timer(0.48f, Timer::FUNC_SINE_IN));
}

void MusicMenu::OnOptions()
{
	paf::vector<OptionMenu::Button> buttons;
	OptionMenu::Button exportSong, minimize, waveform, screenOff;
	exportSong.label = g_appPlugin->GetString(msg_music_export);
	minimize.label = g_appPlugin->GetString(msg_music_background);
	waveform.label = g_appPlugin->GetString(msg_music_waveform);
	screenOff.label = g_appPlugin->GetString(msg_music_screen_off);
	buttons.push_back(exportSong);
	buttons.push_back(minimize);
	buttons.push_back(waveform);
	buttons.push_back(screenOff);
	OptionMenu::Create(g_appPlugin, m_root, &buttons, 0xd098335eu, OptionTag);
}

void MusicMenu::OnOption(int32_t index)
{
	if (index == 0) ExportToMusic();
	else if (index == 1) ListenInBackground();
	else if (index == 2) ToggleWaveform();
	else if (index == 3) EnableScreenOff();
}

void MusicMenu::OnSeekChanged()
{
	if (!m_playing || m_seconds <= 0) return;
	m_seekChanging = true;
	float percent = m_seekSlider->GetValue() / 100.0f;
	if (percent < 0.0f) percent = 0.0f;
	if (percent > 1.0f) percent = 1.0f;
	UpdateSeekPreview(percent);
	musicSeek(percent * m_seconds);
	m_seekChanging = false;
}

bool MusicMenu::HasMinimized()
{
	return s_minimizedPlayer != NULL && musicIsPlaying();
}

intrusive_ptr<graph::Surface> MusicMenu::MinimizedCover()
{
	return s_minimizedPlayer ? s_minimizedPlayer->m_coverSurface : intrusive_ptr<graph::Surface>();
}

bool MusicMenu::MinimizedHasCover()
{
	return s_minimizedPlayer && s_minimizedPlayer->m_hasCover;
}

void MusicMenu::RestoreMinimized()
{
	if (!s_minimizedPlayer) return;
	MusicMenu *menu = s_minimizedPlayer;
	s_minimizedPlayer = NULL;
	menu->m_minimized = false;
	menu->m_restorePending = true;
	menu->m_restoreFrames = 40;
	menu->m_parentRoot->SetActivate(false);
	menu->m_root->SetColor(1.0f, 1.0f, 1.0f, 0.0f);
	menu->m_root->Show(common::transition::Type_Reset);
	menu->m_root->SetActivate(true);
	menu->m_root->SetColor(1.0f, 1.0f, 1.0f, 1.0f, new Timer(0.36f, Timer::FUNC_SINE_OUT));
	menu->StartGenie(true);
}

void MusicMenu::Update()
{
	if (m_restorePending && --m_restoreFrames <= 0)
	{
		m_restorePending = false;
		for (int32_t i = 0; i < GenieSlices; ++i) m_genie[i]->Hide(common::transition::Type_Reset);
		m_cover->SetPos(-260.0f, 6.0f, 0.0f);
		m_cover->SetScale(1.0f, 1.0f, 1.0f);
		m_cover->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		m_cover->Show(common::transition::Type_Reset);
		m_coverShadow->Hide(common::transition::Type_Reset);
		m_root->SetActivate(true);
	}
	if (m_minimizePending && --m_minimizeFrames <= 0)
	{
		m_minimizePending = false;
		m_minimized = true;
		s_minimizedPlayer = this;
		m_root->SetActivate(false);
		m_root->Hide(common::transition::Type_Reset);
		m_parentRoot->SetActivate(true);
	}
	++m_frame;
	if (m_waveformVisible && m_frame % 2 == 0) UpdateWaveform();
	if (!m_playing || m_frame % 15 != 0)
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
	if (!m_seekChanging) m_seekSlider->SetValue(pct, false);
}

void MusicMenu::UpdateWaveform()
{
	float current[WaveBars];
	const bool hasPcm = musicIsPlaying() && musicWaveform(current, WaveBars);
	for (int32_t i = 0; i < WaveBars; ++i)
	{
		// Blend neighbouring PCM windows and apply a square-root response. This
		// produces the dense, fluid waveform used by Vita's Music UI instead of
		// a row of sparse, twitching volume meters.
		float sample = hasPcm ? current[i] : 0.0f;
		if (hasPcm && i > 0) sample += current[i - 1] * 0.45f;
		if (hasPcm && i + 1 < WaveBars) sample += current[i + 1] * 0.45f;
		if (hasPcm) sample /= (i > 0 && i + 1 < WaveBars) ? 1.90f : 1.45f;
		float target = sqrtf(sample) * 0.92f;
		if (target < 0.025f) target = 0.025f;
		if (target > 1.0f) target = 1.0f;
		const float response = target > m_waveLevels[i] ? 0.52f : 0.16f;
		m_waveLevels[i] += (target - m_waveLevels[i]) * response;
		m_waveBars[i]->SetSize(3.0f, 3.0f + m_waveLevels[i] * 34.0f, 0.0f);
		m_waveBars[i]->SetColor(1.0f, 1.0f, 1.0f, 0.55f + m_waveLevels[i] * 0.45f);
	}
}

void MusicMenu::UpdateSeekPreview(float percent)
{
	if (m_seconds <= 0) return;
	long target = (long)(percent * m_seconds);
	char text[16];
	snprintf(text, sizeof(text), "%02ld:%02ld", target / 60, target % 60);
	SetText(m_elapsed, text);
}
