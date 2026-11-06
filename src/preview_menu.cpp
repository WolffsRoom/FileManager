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

#include "common.h"
#include "preview_menu.h"

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

	void VideoTask(void *userdata)
	{
		static_cast<PreviewMenu *>(userdata)->OnVideoFrame();
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
	m_player(0), m_videoActive(false), m_paused(false), m_frameShown(false), m_rgba(NULL), m_videoW(0), m_videoH(0)
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

	Open(index);
}

PreviewMenu::~PreviewMenu()
{
	m_loadId++;
	StopVideo();

	Plugin::PageCloseParam closeParam;
	closeParam.fade = true;
	closeParam.fade_time_ms = 200.0f;
	g_appPlugin->PageClose(page_preview, closeParam);
	m_parentRoot->SetActivate(true);
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
		}
		else
		{
			ShowError();
		}
	}
	thread::RMutex::MainThreadMutex()->Unlock();
}

bool PreviewMenu::StartVideo(const char *path)
{
	if (sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER) < 0)
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
	init.numOutputVideoFrameBuffers = 8;
	init.autoStart = SCE_TRUE;

	m_player = sceAvPlayerInit(&init);
	if (m_player <= 0)
	{
		m_player = 0;
		return false;
	}

	if (sceAvPlayerAddSource(m_player, path) < 0)
	{
		sceAvPlayerClose(m_player);
		m_player = 0;
		return false;
	}

	sceAvPlayerSetLooping(m_player, SCE_TRUE);
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
	sceAvPlayerStop(m_player);
	sceAvPlayerClose(m_player);
	VideoCloseFile(NULL);
	m_player = 0;
	m_videoActive = false;
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

	if (!m_frameShown)
	{
		m_frameShown = true;
		m_busy->Stop();
		m_plane->SetTexture(m_videoSurf, 0, 0);
		m_plane->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}
