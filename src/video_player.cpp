#include <sdk_compat.h>
#include <paf.h>
#include <psp2/audioout.h>
#include <psp2/avplayer.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/sysmodule.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video_player.h"

extern "C" int sceAvPlayerStreamCount(SceAvPlayerHandle handle);

using namespace paf;

namespace {
	const uint32_t k_frameAlign = 1024 * 1024;
	const char *k_decoderNames[] = { "native-GXM", "callback-GXM", "callback-CPU" };

	struct CdramEntry { void *ptr; SceUID uid; };
	CdramEntry s_cdram[24];
	int32_t s_cdramCount = 0;

	uint8_t ClampRgb(int value)
	{
		return (uint8_t)(value < 0 ? 0 : value > 255 ? 255 : value);
	}

	void Nv12ToRgba(const uint8_t *nv12, uint32_t *rgba, int32_t width, int32_t height)
	{
		const uint8_t *uv = nv12 + (size_t)width * height;
		for (int32_t y = 0; y < height; ++y)
		{
			for (int32_t x = 0; x < width; ++x)
			{
				const int32_t offset = (y / 2) * width + (x / 2) * 2;
				const int32_t yy = nv12[(size_t)y * width + x];
				const int32_t u = (int32_t)uv[offset] - 128;
				const int32_t v = (int32_t)uv[offset + 1] - 128;
				const uint8_t r = ClampRgb((yy * 298 + v * 409 + 128) >> 8);
				const uint8_t g = ClampRgb((yy * 298 - u * 100 - v * 208 + 128) >> 8);
				const uint8_t b = ClampRgb((yy * 298 + u * 516 + 128) >> 8);
				rgba[(size_t)y * width + x] = 0xff000000u | ((uint32_t)b << 16) |
					((uint32_t)g << 8) | r;
			}
		}
	}
}

VideoPlayer *VideoPlayer::s_active = NULL;

bool AvPlayerHandleValid(SceAvPlayerHandle handle)
{
	const uint32_t bits = (uint32_t)handle;
	return bits != 0 && (bits & 0xFF000000u) != 0x80000000u;
}

VideoPlayer::VideoPlayer() : m_player(0), m_mode(-1), m_startedUs(0), m_duration(0),
	m_haveFrame(false), m_paused(false), m_failed(false), m_width(0), m_height(0),
	m_cpuPixels(NULL), m_audioThread(-1), m_audioRun(false), m_fileFd(-1), m_fileSize(0)
{
	memset(m_errors, 0, sizeof(m_errors));
}

VideoPlayer::~VideoPlayer()
{
	Close();
}

void VideoPlayer::Log(const char *format, ...)
{
	sceIoMkdir("ux0:data/FileManager", 0777);
	SceUID fd = sceIoOpen("ux0:data/FileManager/video_diagnostic.txt",
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
	if (fd < 0) return;
	char line[384];
	va_list args;
	va_start(args, format);
	const int32_t length = vsnprintf(line, sizeof(line), format, args);
	va_end(args);
	if (length > 0) sceIoWrite(fd, line, length < (int32_t)sizeof(line) ? length : (int32_t)sizeof(line) - 1);
	sceIoWrite(fd, "\r\n", 2);
	sceIoClose(fd);
}

void *VideoPlayer::Alloc(void *, uint32_t alignment, uint32_t size)
{
	if (alignment < 16) alignment = 16;
	return memalign(alignment, size);
}

void VideoPlayer::Free(void *, void *ptr)
{
	free(ptr);
}

void *VideoPlayer::GpuAlloc(void *, uint32_t alignment, uint32_t size)
{
	if (alignment < k_frameAlign) alignment = k_frameAlign;
	size = (size + alignment - 1) & ~(alignment - 1);
	SceUID uid = sceKernelAllocMemBlock("FM::VideoFrame",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, size, NULL);
	if (uid < 0) return NULL;
	void *ptr = NULL;
	if (sceKernelGetMemBlockBase(uid, &ptr) < 0 || !ptr)
	{
		sceKernelFreeMemBlock(uid);
		return NULL;
	}
	const SceGxmMemoryAttribFlags attributes = (SceGxmMemoryAttribFlags)(
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE);
	if (sceGxmMapMemory(ptr, size, attributes) < 0)
	{
		sceKernelFreeMemBlock(uid);
		return NULL;
	}
	return ptr;
}

void VideoPlayer::GpuFree(void *, void *ptr)
{
	if (!ptr) return;
	// The most recently presented PAF texture may still be in the display queue.
	sceDisplayWaitVblankStart();
	SceUID uid = sceKernelFindMemBlockByAddr(ptr, 0);
	sceGxmUnmapMemory(ptr);
	if (uid >= 0) sceKernelFreeMemBlock(uid);
}

void *VideoPlayer::CpuFrameAlloc(void *, uint32_t alignment, uint32_t size)
{
	SceUID uid = sceKernelAllocMemBlock("FM::VideoCdram",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, size, NULL);
	if (uid >= 0)
	{
		void *ptr = NULL;
		if (sceKernelGetMemBlockBase(uid, &ptr) >= 0 && ptr && s_cdramCount < 24)
		{
			s_cdram[s_cdramCount].ptr = ptr;
			s_cdram[s_cdramCount++].uid = uid;
			return ptr;
		}
		sceKernelFreeMemBlock(uid);
	}
	if (alignment < 16) alignment = 16;
	return memalign(alignment, size);
}

void VideoPlayer::CpuFrameFree(void *, void *ptr)
{
	if (!ptr) return;
	for (int32_t i = 0; i < s_cdramCount; ++i)
	{
		if (s_cdram[i].ptr == ptr)
		{
			sceKernelFreeMemBlock(s_cdram[i].uid);
			s_cdram[i] = s_cdram[--s_cdramCount];
			return;
		}
	}
	free(ptr);
}

int VideoPlayer::FileOpen(void *arg, const char *filename)
{
	VideoPlayer *self = arg ? static_cast<VideoPlayer *>(arg) : s_active;
	if (!self) return -1;
	self->m_fileFd = sceIoOpen(filename, SCE_O_RDONLY, 0);
	if (self->m_fileFd < 0) return -1;
	SceOff size = sceIoLseek(self->m_fileFd, 0, SCE_SEEK_END);
	if (size < 0) { FileClose(self); return -1; }
	self->m_fileSize = (uint64_t)size;
	sceIoLseek(self->m_fileFd, 0, SCE_SEEK_SET);
	return 0;
}

int VideoPlayer::FileClose(void *arg)
{
	VideoPlayer *self = arg ? static_cast<VideoPlayer *>(arg) : s_active;
	if (!self) return -1;
	if (self->m_fileFd >= 0) sceIoClose(self->m_fileFd);
	self->m_fileFd = -1;
	self->m_fileSize = 0;
	return 0;
}

int VideoPlayer::FileRead(void *arg, uint8_t *buffer, uint64_t position, uint32_t length)
{
	VideoPlayer *self = arg ? static_cast<VideoPlayer *>(arg) : s_active;
	if (!self || self->m_fileFd < 0) return -1;
	if (sceIoLseek(self->m_fileFd, (SceOff)position, SCE_SEEK_SET) < 0) return -1;
	return sceIoRead(self->m_fileFd, buffer, length);
}

uint64_t VideoPlayer::FileSize(void *arg)
{
	VideoPlayer *self = arg ? static_cast<VideoPlayer *>(arg) : s_active;
	return self ? self->m_fileSize : 0;
}

int VideoPlayer::AudioThread(SceSize, void *argp)
{
	(void)argp;
	VideoPlayer *self = s_active;
	int32_t port = -1, oldRate = 0, oldChannels = 0, oldGrain = 0;
	while (self && self->m_audioRun)
	{
		SceAvPlayerFrameInfo frame;
		memset(&frame, 0, sizeof(frame));
		if (AvPlayerHandleValid(self->m_player) && sceAvPlayerGetAudioData(self->m_player, &frame) && frame.pData)
		{
			const int32_t rate = frame.details.audio.sampleRate;
			const int32_t channels = frame.details.audio.channelCount;
			const int32_t grain = channels > 0 ? frame.details.audio.size / (channels * 2) : 0;
			if (rate != oldRate || channels != oldChannels || grain != oldGrain)
			{
				if (port >= 0) sceAudioOutReleasePort(port);
				port = -1;
				oldRate = rate; oldChannels = channels; oldGrain = grain;
				if ((channels == 1 || channels == 2) && rate >= 8000 && rate <= 48000 && grain >= 64 && grain <= 2048)
					port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, grain, rate,
						channels == 1 ? SCE_AUDIO_OUT_MODE_MONO : SCE_AUDIO_OUT_MODE_STEREO);
			}
			if (port >= 0) sceAudioOutOutput(port, frame.pData);
		}
		else sceKernelDelayThread(4000);
	}
	if (port >= 0) sceAudioOutReleasePort(port);
	return sceKernelExitDeleteThread(0);
}

void VideoPlayer::StartAudio()
{
	m_audioRun = true;
	m_audioThread = sceKernelCreateThread("FM::VideoAudio", AudioThread,
		0x10000100, 0x4000, 0, 0, NULL);
	if (m_audioThread >= 0) sceKernelStartThread(m_audioThread, 0, NULL);
	else m_audioRun = false;
}

void VideoPlayer::StopAudio()
{
	m_audioRun = false;
	if (m_audioThread >= 0)
	{
		sceKernelWaitThreadEnd(m_audioThread, NULL, NULL);
		m_audioThread = -1;
	}
}

void VideoPlayer::RefreshStreams()
{
	const int32_t count = sceAvPlayerStreamCount(m_player);
	for (int32_t i = 0; i < count && i < 32; ++i)
	{
		SceAvPlayerStreamInfo info;
		memset(&info, 0, sizeof(info));
		if (sceAvPlayerGetStreamInfo(m_player, i, &info) < 0) continue;
		if (info.duration > m_duration) m_duration = info.duration;
	}
}

bool VideoPlayer::OpenDecoder(int32_t mode)
{
	SceAvPlayerInitData init;
	memset(&init, 0, sizeof(init));
	init.memoryReplacement.objectPointer = this;
	init.memoryReplacement.allocate = Alloc;
	init.memoryReplacement.deallocate = Free;
	init.memoryReplacement.allocateTexture = mode == 2 ? CpuFrameAlloc : GpuAlloc;
	init.memoryReplacement.deallocateTexture = mode == 2 ? CpuFrameFree : GpuFree;
	init.basePriority = 0xA0;
	init.numOutputVideoFrameBuffers = mode == 2 ? 8 : 5; // five GXM buffers
	init.autoStart = SCE_TRUE;
	if (mode != 0)
	{
		init.fileReplacement.objectPointer = this;
		init.fileReplacement.open = FileOpen;
		init.fileReplacement.close = FileClose;
		init.fileReplacement.readOffset = FileRead;
		init.fileReplacement.size = FileSize;
	}
	s_active = this;
	SceAvPlayerHandle handle = sceAvPlayerInit(&init);
	if (!AvPlayerHandleValid(handle))
	{
		m_errors[mode] = handle;
		Log("%s: sceAvPlayerInit error 0x%08X", k_decoderNames[mode], handle);
		return false;
	}
	const int32_t added = sceAvPlayerAddSource(handle, m_path.c_str());
	Log("%s: AddSource=0x%08X", k_decoderNames[mode], added);
	if (added < 0)
	{
		m_errors[mode] = added;
		sceAvPlayerClose(handle);
		FileClose(this);
		return false;
	}
	m_player = handle;
	m_mode = mode;
	m_errors[mode] = 0;
	m_startedUs = sceKernelGetSystemTimeWide();
	RefreshStreams();
	StartAudio();
	Log("%s: player 0x%08X active", k_decoderNames[mode], handle);
	return true;
}

bool VideoPlayer::TryDecoderFrom(int32_t first)
{
	for (int32_t mode = first; mode < 3; ++mode)
		if (OpenDecoder(mode)) return true;
	return false;
}

bool VideoPlayer::Open(const char *path)
{
	Close();
	if (!path || !path[0]) return false;
	m_path = path;
	memset(m_errors, 0, sizeof(m_errors));
	m_failed = false;
	sceIoMkdir("ux0:data/FileManager", 0777);
	sceIoRemove("ux0:data/FileManager/video_diagnostic.txt");
	const int32_t loaded = sceSysmoduleIsLoaded(SCE_SYSMODULE_AVPLAYER);
	if (loaded < 0) Log("AvPlayer module load=0x%08X", sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER));
	Log("Opening %s", path);
	if (!TryDecoderFrom(0))
	{
		m_failed = true;
		Log("All decoders failed N:%08X C:%08X CPU:%08X", m_errors[0], m_errors[1], m_errors[2]);
		return false;
	}
	return true;
}

void VideoPlayer::CloseDecoder()
{
	StopAudio();
	m_frame.clear();
	m_cpuSurface.clear();
	m_surfaces.clear();
	free(m_cpuPixels);
	m_cpuPixels = NULL;
	m_haveFrame = false;
	m_width = m_height = 0;
	if (AvPlayerHandleValid(m_player))
	{
		sceDisplayWaitVblankStart();
		sceAvPlayerStop(m_player);
		sceAvPlayerClose(m_player);
	}
	m_player = 0;
	FileClose(this);
	m_mode = -1;
}

void VideoPlayer::Close()
{
	CloseDecoder();
	m_path.clear();
	m_duration = 0;
	m_paused = false;
	m_failed = false;
	if (s_active == this) s_active = NULL;
}

void VideoPlayer::Update()
{
	if (!AvPlayerHandleValid(m_player)) return;
	if (!m_haveFrame && m_mode >= 0 && sceKernelGetSystemTimeWide() - m_startedUs >= 5000000ULL)
	{
		const int32_t next = m_mode + 1;
		Log("No video frame in 5 seconds; switching from mode %d", m_mode);
		CloseDecoder();
		if (next >= 3 || !TryDecoderFrom(next))
		{
			m_failed = true;
			Log("Frame timeout N:%08X C:%08X CPU:%08X", m_errors[0], m_errors[1], m_errors[2]);
		}
		return;
	}
	if (!m_duration) RefreshStreams();
	SceAvPlayerFrameInfo frame;
	memset(&frame, 0, sizeof(frame));
	if (!sceAvPlayerGetVideoData(m_player, &frame) || !frame.pData) return;
	const int32_t width = frame.details.video.width;
	const int32_t height = frame.details.video.height;
	if (width <= 0 || height <= 0 || width > 1920 || height > 1088 || (width & 1) || (height & 1))
	{
		Log("Invalid decoded dimensions %dx%d", width, height);
		return;
	}
	if (m_mode == 2)
	{
		if (!m_cpuSurface.get() || width != m_width || height != m_height)
		{
			free(m_cpuPixels);
			m_cpuPixels = (uint32_t *)memalign(64, (size_t)width * height * 4);
			if (!m_cpuPixels) { Log("CPU frame allocation failed: %dx%d", width, height); return; }
			m_cpuSurface = new graph::Surface(gutil::GetDefaultSurfacePool(), width, height,
				ImageMode_RGBA8888, ImageOrder_Linear, 1, 1, false);
		}
		if (!m_cpuSurface.get() || !m_cpuPixels) return;
		Nv12ToRgba(frame.pData, m_cpuPixels, width, height);
		m_cpuSurface->Copy(0, m_cpuPixels, ImageOrder_Linear, width * 4, 0);
		m_frame = m_cpuSurface;
	}
	else
	{
		if (width != m_width || height != m_height) m_surfaces.clear();
		intrusive_ptr<graph::Surface> surface;
		for (uint32_t i = 0; i < m_surfaces.size(); ++i)
			if (m_surfaces.at(i).data == frame.pData) { surface = m_surfaces.at(i).surface; break; }
		if (!surface.get())
		{
			SurfaceEntry entry;
			entry.data = frame.pData;
			entry.surface = new graph::Surface(width, height, ImageMode_YUV420P2,
				ImageOrder_Linear, 1, frame.pData, width, 1, false);
			surface = entry.surface;
			m_surfaces.push_back(entry);
		}
		m_frame = surface;
	}
	m_width = width;
	m_height = height;
	if (!m_haveFrame) Log("First frame %dx%d mode=%d", width, height, m_mode);
	m_haveFrame = m_frame.get() != NULL;
}

void VideoPlayer::PauseResume()
{
	if (!AvPlayerHandleValid(m_player)) return;
	const int32_t result = m_paused ? sceAvPlayerResume(m_player) : sceAvPlayerPause(m_player);
	if (result >= 0) m_paused = !m_paused;
}

void VideoPlayer::Seek(int32_t deltaMs)
{
	int64_t target = (int64_t)CurrentTime() + deltaMs;
	if (target < 0) target = 0;
	if (m_duration && (uint64_t)target > m_duration) target = m_duration > 1000 ? m_duration - 1000 : 0;
	SeekTo((uint64_t)target);
}

void VideoPlayer::SeekTo(uint64_t ms)
{
	if (AvPlayerHandleValid(m_player)) sceAvPlayerJumpToTime(m_player, ms);
}

uint64_t VideoPlayer::CurrentTime() const
{
	return AvPlayerHandleValid(m_player) ? sceAvPlayerCurrentTime(m_player) : 0;
}
