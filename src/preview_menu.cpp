#include <sdk_compat.h>
#include <kernel.h>
#include <paf.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <strings.h>
#include <psp2/avplayer.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/audioout.h>
#include <psp2/touch.h>
#include <math.h>
#include <stdio.h>

// In the stub (NID 0x2CD86FD6) but not in the VitaSDK header.
extern "C" int sceAvPlayerStreamCount(SceAvPlayerHandle handle);
extern "C" int sceAvPlayerStart(SceAvPlayerHandle handle);

#include "common.h"
#include "preview_menu.h"
#include "option_menu.h"

using namespace paf;

namespace {

	// Larger files are almost certainly larger images than the surface pool can hold.
	const int64_t k_maxImageFile = 24 * 1024 * 1024;

	// Video memory and file callbacks follow FMVita's videoplayer.c. SceAvPlayer's C
	// API carries no state we need, and only one preview is ever open.
	enum { MAX_FRAME_ALLOCS = 16 };
	struct FrameAlloc { void *ptr; SceUID uid; };
	FrameAlloc s_frames[MAX_FRAME_ALLOCS];
	int s_frameCount = 0;
	SceUID s_videoFd = -1;
	SceOff s_videoSize = 0;

	void *VideoAlloc(void *arg, uint32_t alignment, uint32_t size)
	{
		return memalign(alignment, size);
	}

	void VideoFree(void *arg, void *ptr)
	{
		free(ptr);
	}

	void *VideoAllocFrame(void *arg, uint32_t alignment, uint32_t size)
	{
		SceUID block = sceKernelAllocMemBlock("fm_avp_frame", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, size, NULL);
		if (block < 0)
		{
			return memalign(alignment, size);
		}
		void *base = NULL;
		if (sceKernelGetMemBlockBase(block, &base) < 0 || !base)
		{
			sceKernelFreeMemBlock(block);
			return memalign(alignment, size);
		}
		if (s_frameCount < MAX_FRAME_ALLOCS)
		{
			s_frames[s_frameCount].ptr = base;
			s_frames[s_frameCount].uid = block;
			s_frameCount++;
		}
		return base;
	}

	void VideoFreeFrame(void *arg, void *ptr)
	{
		if (!ptr)
		{
			return;
		}
		for (int i = 0; i < s_frameCount; i++)
		{
			if (s_frames[i].ptr == ptr)
			{
				sceKernelFreeMemBlock(s_frames[i].uid);
				s_frames[i] = s_frames[--s_frameCount];
				return;
			}
		}
		free(ptr);
	}

	int VideoOpenFile(void *p, const char *filename)
	{
		s_videoFd = sceIoOpen(filename, SCE_O_RDONLY, 0);
		if (s_videoFd < 0)
		{
			return -1;
		}
		s_videoSize = sceIoLseek(s_videoFd, 0, SCE_SEEK_END);
		sceIoLseek(s_videoFd, 0, SCE_SEEK_SET);
		return 0;
	}

	int VideoCloseFile(void *p)
	{
		if (s_videoFd >= 0)
		{
			sceIoClose(s_videoFd);
			s_videoFd = -1;
		}
		return 0;
	}

	int VideoReadOffset(void *p, uint8_t *buffer, uint64_t position, uint32_t length)
	{
		if (s_videoFd < 0)
		{
			return -1;
		}
		sceIoLseek(s_videoFd, position, SCE_SEEK_SET);
		return sceIoRead(s_videoFd, buffer, length);
	}

	uint64_t VideoSizeFile(void *p)
	{
		return s_videoSize;
	}

	inline uint32_t Clamp8(int v)
	{
		return v < 0 ? 0 : (v > 255 ? 255 : (uint32_t)v);
	}

	// Planar YUV420 to RGBA at half resolution: a quarter of the pixels, cheap enough
	// to run on the main thread at video frame rate.
	void ConvertHalf(const uint8_t *yuv, uint32_t *rgba, int w, int h)
	{
		const uint8_t *yp = yuv;
		const uint8_t *up = yuv + w * h;
		const uint8_t *vp = up + (w / 2) * (h / 2);
		const int cw = w / 2;
		for (int j = 0; j < h / 2; j++)
		{
			const uint8_t *yr = yp + j * 2 * w;
			const uint8_t *ur = up + j * cw;
			const uint8_t *vr = vp + j * cw;
			uint32_t *out = rgba + j * cw;
			for (int i = 0; i < cw; i++)
			{
				int Y = yr[i * 2];
				int U = ur[i] - 128;
				int V = vr[i] - 128;
				uint32_t R = Clamp8((Y * 298 + V * 409 + 128) >> 8);
				uint32_t G = Clamp8((Y * 298 - U * 100 - V * 208 + 128) >> 8);
				uint32_t B = Clamp8((Y * 298 + U * 516 + 128) >> 8);
				out[i] = 0xFF000000u | (B << 16) | (G << 8) | R;
			}
		}
	}

	// What the player returned at each step: why a video does not open on the console.
	void VideoLog(const char *fmt, int32_t value)
	{
		SceUID fd = sceIoOpen("ux0:data/FileManager/video.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
		if (fd >= 0)
		{
			char line[160];
			int n = snprintf(line, sizeof(line), fmt, value);
			sceIoWrite(fd, line, n);
			sceIoWrite(fd, "\n", 1);
			sceIoClose(fd);
		}
	}

	// Audio: SceAvPlayer decodes it, this thread feeds the speaker.
	volatile bool s_audioRun = false;
	volatile int32_t s_audioPlayer = 0;
	SceUID s_audioThread = -1;

	int AudioThread(SceSize args, void *argp)
	{
		int port = -1;
		int portRate = 0, portChannels = 0;
		while (s_audioRun)
		{
			SceAvPlayerFrameInfo frame;
			sce_paf_memset(&frame, 0, sizeof(frame));
			if (s_audioPlayer && sceAvPlayerIsActive(s_audioPlayer) && sceAvPlayerGetAudioData(s_audioPlayer, &frame) && frame.pData)
			{
				int rate = frame.details.audio.sampleRate ? frame.details.audio.sampleRate : 48000;
				int channels = frame.details.audio.channelCount ? frame.details.audio.channelCount : 2;
				if (port < 0 || rate != portRate || channels != portChannels)
				{
					if (port >= 0)
					{
						sceAudioOutReleasePort(port);
					}
					port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, 1024, rate,
						channels == 1 ? SCE_AUDIO_OUT_MODE_MONO : SCE_AUDIO_OUT_MODE_STEREO);
					portRate = rate;
					portChannels = channels;
				}
				if (port >= 0)
				{
					sceAudioOutOutput(port, frame.pData);
				}
			}
			else
			{
				sceKernelDelayThread(5000);
			}
		}
		if (port >= 0)
		{
			sceAudioOutOutput(port, NULL);
			sceAudioOutReleasePort(port);
		}
		return sceKernelExitDeleteThread(0);
	}

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

	bool IsVideo(const char *path)
	{
		const char *dot = strrchr(path, '.');
		return dot && strcasecmp(dot, ".mp4") == 0;
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

PreviewMenu::PreviewMenu(ui::Widget *parentRoot, const paf::vector<paf::string> &paths, int32_t index) :
	m_parentRoot(parentRoot), m_paths(paths), m_index(index), m_loadId(0), m_jobId(0),
	m_player(0), m_videoActive(false), m_imageActive(false), m_paused(false), m_frameShown(false), m_rgba(NULL), m_videoW(0), m_videoH(0),
	m_touchActive(false), m_pinchActive(false), m_touchStartX(0), m_touchStartY(0), m_touchLastX(0), m_touchLastY(0),
	m_pinchStart(0), m_pinchScale(1), m_imageScale(1), m_imageInitialScale(1), m_imageX(0), m_imageY(0), m_flipX(false), m_flipY(false)
{
	Plugin::PageOpenParam openParam;
	openParam.graphics_flag = 0x80;
	openParam.overwrite_draw_priority = 8;
	m_root = g_appPlugin->PageOpen(page_preview, openParam);

	m_parentRoot->SetActivate(false);

	m_plane = m_root->FindChild(plane_image_page_preview);
	m_busy = static_cast<ui::BusyIndicator *>(m_root->FindChild(busyindicator_page_preview));
	m_title = static_cast<ui::Text *>(m_root->FindChild(text_title_page_preview));
	m_error = static_cast<ui::Text *>(m_root->FindChild(text_error_page_preview));

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

	m_controls = m_root->FindChild(plane_controls_page_preview);
	m_time = m_controls->FindChild(text_time_page_preview);
	m_barFill = m_controls->FindChild(plane_bar_fill_page_preview);
	m_controls->Hide(common::transition::Type_Reset);

	AddKey(m_root->FindChild(button_rewind_page_preview), inputdevice::pad::Data::PAD_LEFT,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<PreviewMenu *>(userdata)->SeekVideo(-10000);
	}, this);
	AddKey(m_root->FindChild(button_forward_page_preview), inputdevice::pad::Data::PAD_RIGHT,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<PreviewMenu *>(userdata)->SeekVideo(10000);
	}, this);

	AddKey(m_root->FindChild(button_pause_page_preview), inputdevice::pad::Data::PAD_ENTER,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		PreviewMenu *menu = reinterpret_cast<PreviewMenu *>(userdata);
		if (menu->m_videoActive)
		{
			menu->m_paused = !menu->m_paused;
			if (menu->m_paused)
			{
				sceAvPlayerPause(menu->m_player);
			}
			else
			{
				sceAvPlayerResume(menu->m_player);
			}
		}
	}, this);

	common::MainThreadCallList::Register(ImageTouchTask, this);

	Open(index);
}

PreviewMenu::~PreviewMenu()
{
	m_loadId++;
	common::MainThreadCallList::Unregister(ImageTouchTask, this);
	StopVideo();

	Plugin::PageCloseParam closeParam;
	closeParam.fade = true;
	closeParam.fade_time_ms = 200.0f;
	g_appPlugin->PageClose(page_preview, closeParam);
	m_parentRoot->SetActivate(true);
}

void PreviewMenu::SeekVideo(int32_t deltaMs)
{
	if (!m_videoActive)
	{
		return;
	}
	int64_t target = (int64_t)sceAvPlayerCurrentTime(m_player) + deltaMs;
	if (target < 0)
	{
		target = 0;
	}
	if (m_duration && (uint64_t)target > m_duration)
	{
		target = m_duration - 1000;
	}
	sceAvPlayerJumpToTime(m_player, (uint64_t)target);
}

void PreviewMenu::Close()
{
	m_loadId++;
	StopVideo();
	m_root->SetActivate(false);
	DeferDelete(this);
}

void PreviewMenu::ShowError()
{
	m_busy->Stop();
	m_error->Show(common::transition::Type_Reset);
}

void PreviewMenu::Open(int32_t index)
{
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
	m_plane->SetScale(1.0f, 1.0f, 1.0f);
	m_plane->SetPos(0.0f, 0.0f, 0.0f);
	m_index = index;
	m_loadId++;
	m_error->Hide(common::transition::Type_Reset);

	const paf::string &path = m_paths.at(index);

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
	common::SharedPtr<job::JobItem> param(new ImageJob(this));
	g_jobQueue->Enqueue(param);
}

void PreviewMenu::LoadImageJob()
{
	const int32_t id = m_jobId;
	paf::string path = m_jobPath;

	intrusive_ptr<graph::Surface> tex;
	SceIoStat stat;
	sce_paf_memset(&stat, 0, sizeof(stat));
	if (sceIoGetstat(path.c_str(), &stat) >= 0 && stat.st_size <= k_maxImageFile)
	{
		int32_t res = -1;
		auto file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &res);
		if (res == 0)
		{
			tex = graph::Surface::Load(gutil::GetDefaultSurfacePool(), (common::SharedPtr<File> &)file);
		}
	}

	thread::RMutex::MainThreadMutex()->Lock();
	// The page may have moved on to another file, or closed, while this one decoded.
	if (id == m_loadId)
	{
		m_busy->Stop();
		if (tex.get())
		{
			m_plane->SetTexture(tex, 0, 0);
			m_plane->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			m_imageInitialScale = (tex->GetWidth() >= 960 || tex->GetHeight() >= 544) ? 0.7f : 1.0f;
			m_imageScale = m_imageInitialScale;
			ApplyImageTransform();
			m_imageActive = true;
		}
		else
		{
			ShowError();
		}
	}
	thread::RMutex::MainThreadMutex()->Unlock();
}

void PreviewMenu::ApplyImageTransform()
{
	m_plane->SetScale(m_flipX ? -m_imageScale : m_imageScale,
		m_flipY ? -m_imageScale : m_imageScale, 1.0f);
}

void PreviewMenu::OnOptions()
{
	if (!m_imageActive || m_videoActive) return;
	paf::vector<OptionMenu::Button> buttons;
	OptionMenu::Button mirror, invert;
	mirror.label = g_appPlugin->GetString(msg_preview_mirror);
	invert.label = g_appPlugin->GetString(msg_preview_invert);
	buttons.push_back(mirror);
	buttons.push_back(invert);
	new OptionMenu(g_appPlugin, m_root, &buttons, 0xd098335eu, OptionTag);
}

void PreviewMenu::OnOption(int32_t index)
{
	if (index == 0) m_flipX = !m_flipX;
	else if (index == 1) m_flipY = !m_flipY;
	ApplyImageTransform();
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
	sceIoRemove("ux0:data/FileManager/video.log");
	int32_t mod = sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);
	VideoLog("load AVPLAYER: 0x%08X", mod);
	if (mod < 0)
	{
		return false;
	}

	s_frameCount = 0;

	SceAvPlayerInitData init;
	sce_paf_memset(&init, 0, sizeof(init));
	init.memoryReplacement.allocate = VideoAlloc;
	init.memoryReplacement.deallocate = VideoFree;
	init.memoryReplacement.allocateTexture = VideoAllocFrame;
	init.memoryReplacement.deallocateTexture = VideoFreeFrame;
	init.fileReplacement.open = VideoOpenFile;
	init.fileReplacement.close = VideoCloseFile;
	init.fileReplacement.readOffset = VideoReadOffset;
	init.fileReplacement.size = VideoSizeFile;
	init.basePriority = 0x10000100;
	init.numOutputVideoFrameBuffers = 16;
	init.autoStart = SCE_FALSE;

	m_player = sceAvPlayerInit(&init);
	VideoLog("sceAvPlayerInit: 0x%08X", m_player);
	if (m_player < 0)
	{
		m_player = 0;
		return false;
	}

	int32_t added = sceAvPlayerAddSource(m_player, path);
	VideoLog("sceAvPlayerAddSource: 0x%08X", added);
	if (added < 0)
	{
		sceAvPlayerClose(m_player);
		m_player = 0;
		return false;
	}
	int32_t started = sceAvPlayerStart(m_player);
	VideoLog("sceAvPlayerStart: 0x%08X", started);
	if (started < 0)
	{
		sceAvPlayerClose(m_player);
		m_player = 0;
		return false;
	}

	// Duration from the video stream, for the progress bar.
	m_duration = 0;
	int32_t streams = sceAvPlayerStreamCount(m_player);
	VideoLog("streams: %d", streams);
	for (int32_t i = 0; i < streams; i++)
	{
		SceAvPlayerStreamInfo info;
		sce_paf_memset(&info, 0, sizeof(info));
		if (sceAvPlayerGetStreamInfo(m_player, i, &info) >= 0 && info.duration > m_duration)
		{
			m_duration = info.duration;
		}
	}

	s_audioPlayer = m_player;
	s_audioRun = true;
	s_audioThread = sceKernelCreateThread("FM::VideoAudio", AudioThread, 0x10000100, 0x4000, 0, 0, NULL);
	if (s_audioThread >= 0)
	{
		sceKernelStartThread(s_audioThread, 0, NULL);
	}

	sceAvPlayerSetLooping(m_player, SCE_TRUE);
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
	s_audioRun = false;
	if (s_audioThread >= 0)
	{
		sceKernelWaitThreadEnd(s_audioThread, NULL, NULL);
		s_audioThread = -1;
	}
	s_audioPlayer = 0;
	sceAvPlayerStop(m_player);
	sceAvPlayerClose(m_player);
	VideoCloseFile(NULL);
	m_player = 0;
	m_videoActive = false;
	m_controls->Hide(common::transition::Type_Reset);
	free(m_rgba);
	m_rgba = NULL;
	m_videoW = m_videoH = 0;
}

void PreviewMenu::OnVideoFrame()
{
	if (!m_videoActive || !sceAvPlayerIsActive(m_player))
	{
		return;
	}

	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);

	SceAvPlayerFrameInfo frame;
	sce_paf_memset(&frame, 0, sizeof(frame));
	if (sceAvPlayerGetVideoData(m_player, &frame) != SCE_TRUE || !frame.pData)
	{
		return;
	}

	const int w = frame.details.video.width;
	const int h = frame.details.video.height;
	if (w <= 0 || h <= 0)
	{
		return;
	}

	if (w != m_videoW || h != m_videoH)
	{
		free(m_rgba);
		m_rgba = (uint32_t *)memalign(64, (w / 2) * (h / 2) * 4);
		m_videoW = w;
		m_videoH = h;
		m_videoSurf.clear();
		if (m_rgba)
		{
			m_videoSurf = new graph::Surface(gutil::GetDefaultSurfacePool(), w / 2, h / 2,
				ImageMode_RGBA8888, ImageOrder_Linear, 1, 1, 0);
		}
	}
	if (!m_rgba || !m_videoSurf.get())
	{
		return;
	}

	ConvertHalf((const uint8_t *)frame.pData, m_rgba, w, h);
	m_videoSurf->Copy(0, m_rgba, ImageOrder_Linear, (w / 2) * 4, 0);

	if (m_frameCounter++ % 15 == 0)
	{
		uint64_t now = sceAvPlayerCurrentTime(m_player);
		char a[16], b[16], line[40];
		FormatTime(a, sizeof(a), now);
		FormatTime(b, sizeof(b), m_duration);
		snprintf(line, sizeof(line), m_duration ? "%s / %s" : "%s", a, b);
		paf::wstring text;
		common::Utf8ToUtf16(line, &text);
		m_time->SetString(text);
		if (m_duration)
		{
			float w = 600.0f * (float)((double)now / (double)m_duration);
			m_barFill->SetSize(w > 600.0f ? 600.0f : w, 8.0f, 0.0f);
		}
	}

	if (!m_frameShown)
	{
		m_frameShown = true;
		m_busy->Stop();
		m_plane->SetTexture(m_videoSurf, 0, 0);
		m_plane->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}
