#include <sdk_compat.h>
#include <kernel.h>
#include <paf.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <psp2/touch.h>
#include <math.h>
#include <stdio.h>

#include "common.h"
#include "dialog.h"
#include "settings.h"
#include "preview_menu.h"
#include "option_menu.h"
#include "text_input.h"
#include "core/fs.h"
#include "core/gif.h"
#include "core/music.h"
#include "core/media_export.h"
#include "video_player.h"

using namespace paf;

namespace {

	// Larger files are almost certainly larger images than the surface pool can hold.
	const int64_t k_maxImageFile = 24 * 1024 * 1024;

	void FormatTime(char *out, size_t size, uint64_t ms)
	{
		uint32_t s = (uint32_t)(ms / 1000);
		if (s >= 3600)
		{
			snprintf(out, size, "%u:%02u:%02u", s / 3600, (s / 60) % 60, s % 60);
		}
		else
		{
			snprintf(out, size, "%02u:%02u", s / 60, s % 60);
		}
	}

	void VideoTask(void *userdata)
	{
		static_cast<PreviewMenu *>(userdata)->OnVideoFrame();
	}

	void ImageTouchTask(void *userdata)
	{
		static_cast<PreviewMenu *>(userdata)->OnImageTouch();
	}

	void GifTask(void *userdata)
	{
		static_cast<PreviewMenu *>(userdata)->OnGifFrame();
	}

	bool IsVideo(const char *path)
	{
		const char *dot = strrchr(path, '.');
		return dot && strcasecmp(dot, ".mp4") == 0;
	}

	bool IsGif(const char *path)
	{
		const char *dot = strrchr(path, '.');
		return dot && strcasecmp(dot, ".gif") == 0;
	}

	intrusive_ptr<graph::Surface> LoadImageSurface(const char *path)
	{
		intrusive_ptr<graph::Surface> tex;
		SceIoStat stat;
		sce_paf_memset(&stat, 0, sizeof(stat));
		if (sceIoGetstat(path, &stat) < 0 || stat.st_size > k_maxImageFile) return tex;
		int32_t res = -1;
		auto file = LocalFile::Open(path, SCE_O_RDONLY, 0, &res);
		if (res == 0) tex = graph::Surface::Load(gutil::GetDefaultSurfacePool(), (common::SharedPtr<File> &)file);
		return tex;
	}

	class ImageJob : public job::JobItem
	{
	public:

		ImageJob(PreviewMenu *menu) : job::JobItem("FM::ImageJob", NULL), m_menu(menu) {}

		int32_t Run()
		{
			m_menu->LoadImageJob();
			return SCE_PAF_OK;
		}

		void Finish(int32_t result) {}

	private:

		PreviewMenu *m_menu;
	};

	void AddKey(ui::Widget *button, uint32_t key, ui::HandlerCB cb, void *userdata)
	{
		button->SetKeycode(key);
		button->AddEventCallback(ui::Button::CB_BTN_DECIDE, cb, userdata);
	}
}

PreviewMenu::PreviewMenu(ui::Widget *parentRoot, const paf::vector<paf::string> &paths,
	int32_t index, ChangeCallback onChanged, void *userdata) :
	m_parentRoot(parentRoot), m_onChanged(onChanged), m_userdata(userdata), m_filesChanged(false),
	m_paths(paths), m_index(index), m_loadId(0), m_imageJobs(0), m_closePending(false), m_jobId(0),
	m_videoActive(false), m_imageActive(false), m_gifActive(false), m_gifIndex(0), m_gifTick(0), m_paused(false), m_frameShown(false),
	m_touchActive(false), m_pinchActive(false), m_touchStartX(0), m_touchStartY(0), m_touchLastX(0), m_touchLastY(0),
	m_pinchStart(0), m_pinchScale(1), m_imageScale(1), m_imageInitialScale(1), m_imageX(0), m_imageY(0), m_flipX(false), m_flipY(false), m_viewMode(0), m_deletePending(false), m_animateNextImage(false), m_musicWasPlaying(false)
{
	Plugin::PageOpenParam openParam;
	openParam.graphics_flag = 0x80;
	openParam.overwrite_draw_priority = 8;
	m_root = g_appPlugin->PageOpen(page_preview, openParam);

	m_parentRoot->SetActivate(false);

	m_background = m_root->FindChild(plane_black_page_preview);
	m_plane = m_root->FindChild(plane_image_page_preview);
	m_busy = static_cast<ui::BusyIndicator *>(m_root->FindChild(busyindicator_page_preview));
	m_title = static_cast<ui::Text *>(m_root->FindChild(text_title_page_preview));
	m_error = static_cast<ui::Text *>(m_root->FindChild(text_error_page_preview));
	m_leftArrow = m_root->FindChild(plane_arrow_left_page_preview);
	m_rightArrow = m_root->FindChild(plane_arrow_right_page_preview);
	m_thumbLeft = m_root->FindChild(plane_thumb_left_page_preview);
	m_thumbCurrent = m_root->FindChild(plane_thumb_current_page_preview);
	m_thumbRight = m_root->FindChild(plane_thumb_right_page_preview);
	m_thumbLeftButton = m_root->FindChild(button_thumb_left_page_preview);
	m_thumbCurrentButton = m_root->FindChild(button_thumb_current_page_preview);
	m_thumbRightButton = m_root->FindChild(button_thumb_right_page_preview);
	m_thumbLeft->Hide(common::transition::Type_Reset);
	m_thumbCurrent->Hide(common::transition::Type_Reset);
	m_thumbRight->Hide(common::transition::Type_Reset);
	m_thumbLeftButton->Hide(common::transition::Type_Reset);
	m_thumbCurrentButton->Hide(common::transition::Type_Reset);
	m_thumbRightButton->Hide(common::transition::Type_Reset);
	AddKey(m_thumbLeftButton, 0,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata) { reinterpret_cast<PreviewMenu *>(userdata)->OnThumbnail(-1); }, this);
	AddKey(m_thumbCurrentButton, 0,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata) { reinterpret_cast<PreviewMenu *>(userdata)->OnThumbnail(0); }, this);
	AddKey(m_thumbRightButton, 0,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata) { reinterpret_cast<PreviewMenu *>(userdata)->OnThumbnail(1); }, this);
	if (m_paths.size() < 2)
	{
		m_leftArrow->Hide(common::transition::Type_Reset);
		m_rightArrow->Hide(common::transition::Type_Reset);
	}

	paf::wstring errorText = g_appPlugin->GetString(msg_preview_error);
	m_error->SetString(errorText);
	m_error->Hide(common::transition::Type_Reset);

	graph::PlaneObj *po = static_cast<graph::PlaneObj *>(m_plane->GetDrawObj(ui::Plane::OBJ_PLANE));
	po->SetScaleMode(graph::PlaneObj::SCALE_ASPECT_SIZE, graph::PlaneObj::SCALE_ASPECT_SIZE);

	AddKey(m_root->FindChild(button_back_page_preview), inputdevice::pad::Data::PAD_ESCAPE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		PreviewMenu *menu = reinterpret_cast<PreviewMenu *>(userdata);
		menu->Close();
	}, this);

	ui::Widget *options = m_root->FindChild(button_options_page_preview);
	options->SetKeycode(inputdevice::pad::Data::PAD_MENU);
	options->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<PreviewMenu *>(userdata)->OnOptions();
	}, this);
	m_root->AddEventCallback(OptionMenu::OptionMenuEvent,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		if (e->GetValue(0) == OptionMenu::OptionMenuEvent_Button && e->GetValue(2) == PreviewMenu::OptionTag)
			reinterpret_cast<PreviewMenu *>(userdata)->OnOption(e->GetValue(1));
	}, this);

	AddKey(m_root->FindChild(button_prev_page_preview), inputdevice::pad::Data::PAD_L,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		PreviewMenu *menu = reinterpret_cast<PreviewMenu *>(userdata);
		int32_t n = menu->m_paths.size();
		menu->Open((menu->m_index + n - 1) % n);
	}, this);

	AddKey(m_root->FindChild(button_next_page_preview), inputdevice::pad::Data::PAD_R,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		PreviewMenu *menu = reinterpret_cast<PreviewMenu *>(userdata);
		menu->Open((menu->m_index + 1) % menu->m_paths.size());
	}, this);
	ui::Widget *leftArrowButton = m_root->FindChild(button_arrow_left_page_preview);
	ui::Widget *rightArrowButton = m_root->FindChild(button_arrow_right_page_preview);
	AddKey(leftArrowButton, 0,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata) { PreviewMenu *m = reinterpret_cast<PreviewMenu *>(userdata); m->SetArrowPressed(true, false); m->StepImage(-1); }, this);
	AddKey(rightArrowButton, 0,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata) { PreviewMenu *m = reinterpret_cast<PreviewMenu *>(userdata); m->SetArrowPressed(false, false); m->StepImage(1); }, this);
	leftArrowButton->AddEventCallback(ui::Button::CB_BTN_PRESS,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata) { reinterpret_cast<PreviewMenu *>(userdata)->SetArrowPressed(true, true); }, this);
	leftArrowButton->AddEventCallback(ui::Button::CB_BTN_PRESS_OUT,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata) { reinterpret_cast<PreviewMenu *>(userdata)->SetArrowPressed(true, false); }, this);
	rightArrowButton->AddEventCallback(ui::Button::CB_BTN_PRESS,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata) { reinterpret_cast<PreviewMenu *>(userdata)->SetArrowPressed(false, true); }, this);
	rightArrowButton->AddEventCallback(ui::Button::CB_BTN_PRESS_OUT,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata) { reinterpret_cast<PreviewMenu *>(userdata)->SetArrowPressed(false, false); }, this);
	AddKey(m_root->FindChild(button_down_page_preview), inputdevice::pad::Data::PAD_DOWN,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata)
	{
		PreviewMenu *menu = reinterpret_cast<PreviewMenu *>(userdata);
		if (!menu->m_videoActive) menu->SetViewMode(1);
	}, this);
	AddKey(m_root->FindChild(button_up_page_preview), inputdevice::pad::Data::PAD_UP,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata)
	{
		PreviewMenu *menu = reinterpret_cast<PreviewMenu *>(userdata);
		if (!menu->m_videoActive) menu->SetViewMode(menu->m_viewMode == 1 ? 0 : 2);
	}, this);

	m_controls = m_root->FindChild(plane_controls_page_preview);
	m_time = m_controls->FindChild(text_time_page_preview);
	m_videoSlider = static_cast<ui::ProgressBarTouch *>(m_controls->FindChild(progressbar_video_page_preview));
	m_videoSlider->SetMinValue(0.0f);
	m_videoSlider->SetMaxValue(100.0f);
	m_videoSlider->AddEventCallback(ui::ProgressBarTouch::CB_PROGRESSBAR_CHANGED,
	[](int32_t, ui::Handler *, ui::Event *, void *userdata)
	{
		reinterpret_cast<PreviewMenu *>(userdata)->OnVideoSeek();
	}, this);
	m_controls->Hide(common::transition::Type_Reset);

	AddKey(m_root->FindChild(button_rewind_page_preview), inputdevice::pad::Data::PAD_LEFT,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		PreviewMenu *menu = reinterpret_cast<PreviewMenu *>(userdata);
		if (menu->m_videoActive) menu->SeekVideo(-10000);
		else if (menu->m_paths.size() > 1) menu->Open((menu->m_index + menu->m_paths.size() - 1) % menu->m_paths.size());
	}, this);
	AddKey(m_root->FindChild(button_forward_page_preview), inputdevice::pad::Data::PAD_RIGHT,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		PreviewMenu *menu = reinterpret_cast<PreviewMenu *>(userdata);
		if (menu->m_videoActive) menu->SeekVideo(10000);
		else if (menu->m_paths.size() > 1) menu->Open((menu->m_index + 1) % menu->m_paths.size());
	}, this);

	AddKey(m_root->FindChild(button_pause_page_preview), inputdevice::pad::Data::PAD_ENTER,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		PreviewMenu *menu = reinterpret_cast<PreviewMenu *>(userdata);
		if (menu->m_videoActive)
		{
			menu->m_videoPlayer.PauseResume();
			menu->m_paused = menu->m_videoPlayer.IsPaused();
		}
	}, this);
	m_root->AddEventCallback(dialog::DialogEvent,
	[](int32_t, ui::Handler *, ui::Event *e, void *userdata)
	{
		reinterpret_cast<PreviewMenu *>(userdata)->OnDeleteAnswer(e->GetValue(0));
	}, this);

	common::MainThreadCallList::Register(ImageTouchTask, this);

	Open(index);
}

PreviewMenu::~PreviewMenu()
{
	m_loadId++;
	common::MainThreadCallList::Unregister(ImageTouchTask, this);
	StopGif();
	StopVideo();

	Plugin::PageCloseParam closeParam;
	closeParam.fade = true;
	closeParam.fade_time_ms = 200.0f;
	g_appPlugin->PageClose(page_preview, closeParam);
	m_parentRoot->SetActivate(true);
	// Browser must not continue displaying stale filenames after rename/delete.
	if (m_filesChanged && m_onChanged) m_onChanged(m_userdata);
}

void PreviewMenu::SeekVideo(int32_t deltaMs)
{
	if (!m_videoActive)
	{
		return;
	}
	m_videoPlayer.Seek(deltaMs);
}

void PreviewMenu::OnVideoSeek()
{
	if (!m_videoActive || !m_duration) return;
	float percent = m_videoSlider->GetValue();
	if (percent < 0.0f) percent = 0.0f;
	if (percent > 100.0f) percent = 100.0f;
	m_videoPlayer.SeekTo((uint64_t)((double)m_duration * percent / 100.0));
}

void PreviewMenu::StepImage(int32_t delta)
{
	if (m_videoActive || m_paths.size() < 2) return;
	int32_t n = m_paths.size();
	Open((m_index + n + delta) % n);
}

void PreviewMenu::OnThumbnail(int32_t delta)
{
	if (!m_imageActive) return;
	if (delta == 0)
	{
		SetViewMode(0);
		return;
	}
	m_animateNextImage = true;
	int32_t n = m_paths.size();
	Open((m_index + n + delta) % n);
}

void PreviewMenu::SetArrowPressed(bool left, bool pressed)
{
	ui::Widget *arrow = left ? m_leftArrow : m_rightArrow;
	arrow->SetColor(1.0f, 1.0f, 1.0f, pressed ? 0.28f : 1.0f);
}

void PreviewMenu::SetViewMode(int32_t mode)
{
	if (!m_imageActive) return;
	m_viewMode = mode;
	bool strip = mode == 1;
	bool full = mode == 2;
	if (strip)
	{
		m_thumbLeft->SetTexture(m_leftSurf, 0, 0);
		m_thumbCurrent->SetTexture(m_currentSurf, 0, 0);
		m_thumbRight->SetTexture(m_rightSurf, 0, 0);
		m_thumbLeft->Show(common::transition::Type_FadeinFast);
		m_thumbCurrent->Show(common::transition::Type_FadeinFast);
		m_thumbRight->Show(common::transition::Type_FadeinFast);
		m_thumbLeftButton->Show(common::transition::Type_Reset);
		m_thumbCurrentButton->Show(common::transition::Type_Reset);
		m_thumbRightButton->Show(common::transition::Type_Reset);
		m_thumbLeft->SetScale(0.86f, 0.86f, 1.0f);
		m_thumbCurrent->SetScale(0.86f, 0.86f, 1.0f);
		m_thumbRight->SetScale(0.86f, 0.86f, 1.0f);
		m_thumbLeft->SetScale(1.0f, 1.0f, 1.0f, new Timer(0.24f, Timer::FUNC_SINE_OUT, Timer::REPEAT_NONE));
		m_thumbCurrent->SetScale(1.0f, 1.0f, 1.0f, new Timer(0.28f, Timer::FUNC_SINE_OUT, Timer::REPEAT_NONE));
		m_thumbRight->SetScale(1.0f, 1.0f, 1.0f, new Timer(0.24f, Timer::FUNC_SINE_OUT, Timer::REPEAT_NONE));
		m_imageScale = m_imageInitialScale * 0.72f;
	}
	else
	{
		m_thumbLeft->Hide(common::transition::Type_FadeinFast);
		m_thumbCurrent->Hide(common::transition::Type_FadeinFast);
		m_thumbRight->Hide(common::transition::Type_FadeinFast);
		m_thumbLeftButton->Hide(common::transition::Type_Reset);
		m_thumbCurrentButton->Hide(common::transition::Type_Reset);
		m_thumbRightButton->Hide(common::transition::Type_Reset);
		m_imageScale = full ? m_imageInitialScale * 1.25f : m_imageInitialScale;
	}
	graph::PlaneObj *obj = static_cast<graph::PlaneObj *>(m_plane->GetDrawObj(ui::Plane::OBJ_PLANE));
	obj->SetTexRegion(math::v4(m_flipX ? 1.0f : 0.0f, m_flipY ? 1.0f : 0.0f,
		m_flipX ? -1.0f : 1.0f, m_flipY ? -1.0f : 1.0f));
	m_plane->SetPos(0.0f, strip ? 58.0f : 0.0f, 0.0f,
		new Timer(0.30f, Timer::FUNC_SINE_INOUT, Timer::REPEAT_NONE));
	m_plane->SetScale(m_imageScale, m_imageScale, 1.0f,
		new Timer(0.30f, Timer::FUNC_SINE_INOUT, Timer::REPEAT_NONE));
	if (full)
	{
		m_title->Hide(common::transition::Type_FadeinFast);
		m_leftArrow->Hide(common::transition::Type_FadeinFast);
		m_rightArrow->Hide(common::transition::Type_FadeinFast);
	}
	else
	{
		m_title->Show(common::transition::Type_FadeinFast);
		if (m_paths.size() > 1) { m_leftArrow->Show(common::transition::Type_FadeinFast); m_rightArrow->Show(common::transition::Type_FadeinFast); }
	}
}

void PreviewMenu::Close()
{
	if (m_closePending) return;
	m_closePending = true;
	m_loadId++;
	StopGif();
	StopVideo();
	m_imageActive = false;
	m_root->SetActivate(false);
	if (m_imageJobs == 0) DeferDelete(this);
}

void PreviewMenu::ShowError()
{
	m_busy->Stop();
	m_error->Show(common::transition::Type_Reset);
}

void PreviewMenu::Open(int32_t index)
{
	StopGif();
	StopVideo();
	m_imageActive = false;
	m_touchActive = false;
	m_pinchActive = false;
	m_imageScale = 1.0f;
	m_imageInitialScale = 1.0f;
	m_imageX = 0.0f;
	m_imageY = 0.0f;
	m_flipX = false;
	m_flipY = false;
	m_viewMode = 0;
	m_title->Show(common::transition::Type_Reset);
	m_thumbLeft->Hide(common::transition::Type_Reset);
	m_thumbCurrent->Hide(common::transition::Type_Reset);
	m_thumbRight->Hide(common::transition::Type_Reset);
	m_thumbLeftButton->Hide(common::transition::Type_Reset);
	m_thumbCurrentButton->Hide(common::transition::Type_Reset);
	m_thumbRightButton->Hide(common::transition::Type_Reset);
	m_plane->SetScale(1.0f, 1.0f, 1.0f);
	m_plane->SetPos(0.0f, 0.0f, 0.0f);
	m_index = index;
	m_loadId++;
	m_error->Hide(common::transition::Type_Reset);

	const paf::string &path = m_paths.at(index);
	const float overlayAlpha = IsVideo(path.c_str()) ? 1.0f : settings::PreviewOpacity();
	m_background->SetColor(0.0f, 0.0f, 0.0f, overlayAlpha);

	const char *slash = strrchr(path.c_str(), '/');
	paf::wstring title;
	common::Utf8ToUtf16(slash ? slash + 1 : path.c_str(), &title);
	m_title->SetString(title);

	m_busy->Start();

	if (IsVideo(path.c_str()))
	{
		if (!StartVideo(path.c_str()))
		{
			ShowError();
		}
		return;
	}

	m_jobId = m_loadId;
	m_jobPath = path;
	__sync_add_and_fetch(&m_imageJobs, 1);
	common::SharedPtr<job::JobItem> param(new ImageJob(this));
	g_jobQueue->Enqueue(param);
}

void PreviewMenu::FinishImageJob()
{
	const int32_t remaining = __sync_sub_and_fetch(&m_imageJobs, 1);
	if (remaining == 0 && m_closePending) DeferDelete(this);
}

void PreviewMenu::LoadImageJob()
{
	const int32_t id = m_jobId;
	paf::string path = m_jobPath;

	intrusive_ptr<graph::Surface> tex;
	paf::vector<intrusive_ptr<graph::Surface> > gifFrames;
	paf::vector<int32_t> gifDelays;
	if (IsGif(path.c_str()))
	{
		GifAnimation gif;
		if (gifLoad(path.c_str(), &gif) >= 0)
		{
			const uint64_t frameBytes = (uint64_t)gif.width * gif.height * 4;
			for (int i = 0; i < gif.frameCount; ++i)
			{
				intrusive_ptr<graph::Surface> frame;
				frame = new graph::Surface(gutil::GetDefaultSurfacePool(), gif.width, gif.height,
					ImageMode_RGBA8888, ImageOrder_Linear, 1, 1, false);
				if (!frame.get() || frame->Copy(0, gif.rgba + frameBytes * i,
					ImageOrder_Linear, gif.width * 4, 0) < 0)
				{
					gifFrames.clear();
					gifDelays.clear();
					break;
				}
				gifFrames.push_back(frame);
				gifDelays.push_back(gif.delaysMs[i]);
			}
			gifFree(&gif);
		}
		if (!gifFrames.empty()) tex = gifFrames.at(0);
	}
	else
	{
		tex = LoadImageSurface(path.c_str());
	}
	int32_t n = m_paths.size();
	intrusive_ptr<graph::Surface> left = n > 1 ? LoadImageSurface(m_paths.at((m_index + n - 1) % n).c_str()) : tex;
	intrusive_ptr<graph::Surface> right = n > 1 ? LoadImageSurface(m_paths.at((m_index + 1) % n).c_str()) : tex;

	thread::RMutex::MainThreadMutex()->Lock();
	// The page may have moved on to another file, or closed, while this one decoded.
	if (id == m_loadId)
	{
		m_busy->Stop();
		if (tex.get())
		{
			m_gifFrames = gifFrames;
			m_gifDelays = gifDelays;
			m_gifIndex = 0;
			m_gifTick = sceKernelGetProcessTimeWide();
			m_gifActive = m_gifFrames.size() > 1;
			if (m_gifActive) common::MainThreadCallList::Register(GifTask, this);
			m_currentSurf = tex;
			m_leftSurf = left;
			m_rightSurf = right;
			m_plane->SetTexture(tex, 0, 0);
			m_plane->SetColor(1.0f, 1.0f, 1.0f, m_animateNextImage ? 0.12f : 1.0f);
			float displayW = (float)tex->GetWidth();
			float displayH = (float)tex->GetHeight();
			if (displayW >= 960.0f || displayH >= 544.0f)
			{
				float fit = fminf(960.0f / displayW, 544.0f / displayH) * 0.70f;
				displayW *= fit;
				displayH *= fit;
			}
			m_plane->SetSize(displayW, displayH, 0.0f);
			graph::PlaneObj *obj = static_cast<graph::PlaneObj *>(m_plane->GetDrawObj(ui::Plane::OBJ_PLANE));
			obj->SetScaleMode(graph::PlaneObj::SCALE_ASPECT_SIZE, graph::PlaneObj::SCALE_ASPECT_SIZE);
			m_imageInitialScale = 1.0f;
			m_imageScale = m_animateNextImage ? 0.90f : m_imageInitialScale;
			ApplyImageTransform();
			if (m_animateNextImage)
			{
				m_plane->SetScale(1.0f, 1.0f, 1.0f, new Timer(0.30f, Timer::FUNC_SINE_OUT, Timer::REPEAT_NONE));
				m_plane->SetColor(1.0f, 1.0f, 1.0f, 1.0f, new Timer(0.24f, Timer::FUNC_SINE_OUT, Timer::REPEAT_NONE));
				m_imageScale = 1.0f;
				m_animateNextImage = false;
			}
			m_imageActive = true;
		}
		else
		{
			ShowError();
		}
	}
	thread::RMutex::MainThreadMutex()->Unlock();
	FinishImageJob();
}

void PreviewMenu::StopGif()
{
	if (m_gifActive)
		common::MainThreadCallList::Unregister(GifTask, this);
	m_gifActive = false;
	m_gifFrames.clear();
	m_gifDelays.clear();
	m_gifIndex = 0;
	m_gifTick = 0;
}

void PreviewMenu::OnGifFrame()
{
	if (!m_gifActive || m_gifFrames.size() < 2 || m_gifDelays.empty()) return;
	uint64_t now = sceKernelGetProcessTimeWide();
	uint64_t delay = (uint64_t)m_gifDelays.at(m_gifIndex) * 1000;
	if (now - m_gifTick < delay) return;
	m_gifTick = now;
	m_gifIndex = (m_gifIndex + 1) % m_gifFrames.size();
	m_currentSurf = m_gifFrames.at(m_gifIndex);
	m_plane->SetTexture(m_currentSurf, 0, 0);
}

void PreviewMenu::ApplyImageTransform()
{
	graph::PlaneObj *obj = static_cast<graph::PlaneObj *>(m_plane->GetDrawObj(ui::Plane::OBJ_PLANE));
	obj->SetTexRegion(math::v4(m_flipX ? 1.0f : 0.0f, m_flipY ? 1.0f : 0.0f,
		m_flipX ? -1.0f : 1.0f, m_flipY ? -1.0f : 1.0f));
	m_plane->SetScale(m_imageScale, m_imageScale, 1.0f);
}

void PreviewMenu::OnOptions()
{
	if (m_videoActive)
	{
		paf::vector<OptionMenu::Button> buttons;
		OptionMenu::Button speed, exportVideo;
		const int32_t values[] = { 100, 200 };
		int32_t current = m_videoPlayer.Speed(), next = values[0];
		for (uint32_t i = 0; i < 2; ++i) if (values[i] == current) next = values[(i + 1) % 2];
		if (next == 200) speed.label = g_appPlugin->GetString(msg_video_speed_2);
		else speed.label = g_appPlugin->GetString(msg_video_speed_1);
		exportVideo.label = g_appPlugin->GetString(msg_music_export);
		buttons.push_back(speed);
		buttons.push_back(exportVideo);
		OptionMenu::Create(g_appPlugin, m_root, &buttons, 0xd098335eu, OptionTag);
		return;
	}
	if (!m_imageActive) return;
	paf::vector<OptionMenu::Button> buttons;
	OptionMenu::Button mirror, invert, rename, remove;
	mirror.label = g_appPlugin->GetString(msg_preview_mirror);
	invert.label = g_appPlugin->GetString(msg_preview_invert);
	rename.label = g_appPlugin->GetString(msg_opt_rename);
	remove.label = g_appPlugin->GetString(msg_opt_delete);
	buttons.push_back(mirror);
	buttons.push_back(invert);
	buttons.push_back(rename);
	buttons.push_back(remove);
	OptionMenu::Create(g_appPlugin, m_root, &buttons, 0xd098335eu, OptionTag);
}

void PreviewMenu::OnOption(int32_t index)
{
	if (m_videoActive)
	{
		if (index == 0)
		{
			const int32_t values[] = { 100, 200 };
			int32_t next = 100;
			for (uint32_t i = 0; i < 2; ++i) if (values[i] == m_videoPlayer.Speed()) next = values[(i + 1) % 2];
			int32_t result = m_videoPlayer.SetSpeed(next);
			if (result < 0) dialog::OpenError(g_appPlugin, result);
		}
		else if (index == 1)
		{
			uint64_t value = 0;
			SceIoStat stat; memset(&stat, 0, sizeof(stat));
			sceIoGetstat(m_paths.at(m_index).c_str(), &stat);
			FileProcessParam param = { &value, stat.st_size ? (uint64_t)stat.st_size : 1, NULL, NULL };
			int32_t result = exportMediaFile(m_paths.at(m_index).c_str(), 1, &param);
			if (result < 0) dialog::OpenError(g_appPlugin, result);
		}
		return;
	}
	if (index == 0) m_flipX = !m_flipX;
	else if (index == 1) m_flipY = !m_flipY;
	else if (index == 2)
	{
		const paf::string &path = m_paths.at(m_index);
		const char *slash = strrchr(path.c_str(), '/');
		paf::wstring initial;
		common::Utf8ToUtf16(slash ? slash + 1 : path.c_str(), &initial);
		new TextInput(m_root, g_appPlugin->GetString(msg_rename_title), initial,
		[](const paf::wstring &text, void *userdata) { reinterpret_cast<PreviewMenu *>(userdata)->OnRenameEntered(text); }, this);
		return;
	}
	else if (index == 3)
	{
		m_deletePending = true;
		const paf::string &path = m_paths.at(m_index);
		const char *slash = strrchr(path.c_str(), '/');
		paf::wstring name;
		common::Utf8ToUtf16(slash ? slash + 1 : path.c_str(), &name);
		dialog::OpenYesNo(g_appPlugin, g_appPlugin->GetString(msg_confirm_delete_title), name.c_str());
		dialog::SetProgressIcon(g_appPlugin->GetTexture(tex_new_trash));
		return;
	}
	ApplyImageTransform();
}

void PreviewMenu::OnRenameEntered(const paf::wstring &text)
{
	if (text.empty() || m_index < 0 || m_index >= (int32_t)m_paths.size()) return;
	paf::string name;
	common::Utf16ToUtf8(text, &name);
	if (!fs::IsValidName(name.c_str())) { dialog::OpenError(g_appPlugin, SCE_ERROR_ERRNO_EINVAL); return; }
	paf::string oldPath = m_paths.at(m_index);
	const char *slash = strrchr(oldPath.c_str(), '/');
	if (!slash) return;
	char renamed[1024];
	int32_t parentLen = slash - oldPath.c_str() + 1;
	if (parentLen + (int32_t)name.length() >= (int32_t)sizeof(renamed)) { dialog::OpenError(g_appPlugin, (int32_t)0x80010024); return; }
	memcpy(renamed, oldPath.c_str(), parentLen);
	strcpy(renamed + parentLen, name.c_str());
	int32_t ret = sceIoRename(oldPath.c_str(), renamed);
	if (ret < 0) { dialog::OpenError(g_appPlugin, ret); return; }
	shortcuts::RenamePath(oldPath, paf::string(renamed));
	m_filesChanged = true;
	m_paths.at(m_index) = renamed;
	Open(m_index);
}

void PreviewMenu::OnDeleteAnswer(int32_t buttonCode)
{
	if (!m_deletePending) return;
	m_deletePending = false;
	if (buttonCode != dialog::ButtonCode_Yes || m_index < 0 || m_index >= (int32_t)m_paths.size()) return;
	paf::string removed = m_paths.at(m_index);
	int32_t ret = sceIoRemove(removed.c_str());
	if (ret < 0) { dialog::OpenError(g_appPlugin, ret); return; }
	shortcuts::RemovePath(removed);
	m_filesChanged = true;
	RemoveCurrentPath();
}

void PreviewMenu::RemoveCurrentPath()
{
	if (m_index < 0 || m_index >= (int32_t)m_paths.size()) return;
	m_paths.erase(m_paths.begin() + m_index);
	if (m_paths.empty()) { Close(); return; }
	if (m_index >= (int32_t)m_paths.size()) m_index = m_paths.size() - 1;
	Open(m_index);
}

void PreviewMenu::OnImageTouch()
{
	if (!m_imageActive || m_videoActive)
		return;

	SceTouchData touch;
	memset(&touch, 0, sizeof(touch));
	sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
	if (touch.reportNum == 0)
	{
		if (m_touchActive && !m_pinchActive)
		{
			float dx = m_touchLastX - m_touchStartX;
			float dy = m_touchLastY - m_touchStartY;
			if (fabsf(dx) > 140.0f && fabsf(dy) < 100.0f)
			{
				int32_t n = m_paths.size();
				Open(dx < 0 ? (m_index + 1) % n : (m_index + n - 1) % n);
			}
		}
		m_touchActive = false;
		m_pinchActive = false;
		return;
	}

	float x = touch.report[0].x * (960.0f / 1920.0f);
	float y = touch.report[0].y * (544.0f / 1088.0f);
	if (y < 48.0f || y > 500.0f)
		return;

	if (!m_touchActive)
	{
		m_touchActive = true;
		m_touchStartX = m_touchLastX = x;
		m_touchStartY = m_touchLastY = y;
	}

	if (touch.reportNum > 1)
	{
		float x2 = touch.report[1].x * (960.0f / 1920.0f);
		float y2 = touch.report[1].y * (544.0f / 1088.0f);
		float dx = x2 - x;
		float dy = y2 - y;
		float distance = sqrtf(dx * dx + dy * dy);
		if (!m_pinchActive)
		{
			m_pinchActive = true;
			m_pinchStart = distance;
			m_pinchScale = m_imageScale;
		}
		if (m_pinchStart > 1.0f)
		{
			m_imageScale = m_pinchScale * distance / m_pinchStart;
			if (m_imageScale < 0.3f) m_imageScale = 0.3f;
			if (m_imageScale > 5.0f) m_imageScale = 5.0f;
			ApplyImageTransform();
		}
	}
	m_touchLastX = x;
	m_touchLastY = y;
}

bool PreviewMenu::StartVideo(const char *path)
{
	m_duration = 0;
	m_musicWasPlaying = musicIsPlaying();
	if (m_musicWasPlaying) musicPause();
	if (!m_videoPlayer.Open(path))
	{
		if (m_musicWasPlaying && musicIsOpen()) musicResume();
		m_musicWasPlaying = false;
		return false;
	}
	m_controls->Show(common::transition::Type_Reset);
	m_frameCounter = 0;
	m_videoActive = true;
	m_paused = false;
	m_frameShown = false;
	common::MainThreadCallList::Register(VideoTask, this);
	return true;
}

void PreviewMenu::StopVideo()
{
	if (!m_videoActive)
	{
		return;
	}
	common::MainThreadCallList::Unregister(VideoTask, this);
	m_plane->Hide(common::transition::Type_Reset);
	// Release decoder-backed textures before AvPlayer frees its video pool.
	m_plane->SetTexture(paf::intrusive_ptr<paf::graph::Surface>());
	m_videoPlayer.Close();
	m_videoActive = false;
	m_controls->Hide(common::transition::Type_Reset);
	if (m_musicWasPlaying && musicIsOpen()) musicResume();
	m_musicWasPlaying = false;
}

void PreviewMenu::OnVideoFrame()
{
	if (!m_videoActive)
	{
		return;
	}
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
	m_videoPlayer.Update();
	if (m_videoPlayer.Failed())
	{
		ShowError();
		StopVideo();
		return;
	}
	intrusive_ptr<graph::Surface> surface = m_videoPlayer.Frame();
	if (surface.get()) m_plane->SetTexture(surface, 0, 0);
	else if (!m_videoPlayer.HasFirstFrame()) m_plane->Hide(common::transition::Type_Reset);
	m_duration = m_videoPlayer.Duration();
	m_paused = m_videoPlayer.IsPaused();

	if (m_frameCounter++ % 15 == 0)
	{
		uint64_t now = m_videoPlayer.CurrentTime();
		char a[16], b[16], line[40];
		FormatTime(a, sizeof(a), now);
		FormatTime(b, sizeof(b), m_duration);
		snprintf(line, sizeof(line), m_duration ? "%s / %s" : "%s", a, b);
		paf::wstring text;
		common::Utf8ToUtf16(line, &text);
		m_time->SetString(text);
		if (m_duration)
		{
			float percent = 100.0f * (float)((double)now / (double)m_duration);
			m_videoSlider->SetValue(percent > 100.0f ? 100.0f : percent, false);
		}
	}

	if (!m_frameShown && m_videoPlayer.HasFirstFrame())
	{
		m_frameShown = true;
		m_busy->Stop();
		m_plane->Show(common::transition::Type_Reset);
		m_plane->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}
