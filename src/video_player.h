#ifndef _FM_VIDEO_PLAYER_H_
#define _FM_VIDEO_PLAYER_H_

#include <sdk_compat.h>
#include <paf.h>
#include <psp2/avplayer.h>

bool AvPlayerHandleValid(SceAvPlayerHandle handle);

/** Native local MP4 player with the three decoder paths proven by Wolff's Store. */
class VideoPlayer
{
public:
	VideoPlayer();
	~VideoPlayer();

	bool Open(const char *path);
	void Close();
	void Update();
	void PauseResume();
	void Seek(int32_t deltaMs);
	void SeekTo(uint64_t ms);
	uint64_t CurrentTime() const;
	uint64_t Duration() const { return m_duration; }
	paf::intrusive_ptr<paf::graph::Surface> Frame() const { return m_frame; }
	bool HasFirstFrame() const { return m_haveFrame; }
	bool IsPaused() const { return m_paused; }
	bool Failed() const { return m_failed; }

private:
	struct SurfaceEntry
	{
		SurfaceEntry() : data(NULL) {}
		void *data;
		paf::intrusive_ptr<paf::graph::Surface> surface;
	};

	bool OpenDecoder(int32_t mode);
	bool TryDecoderFrom(int32_t first);
	void CloseDecoder();
	void RefreshStreams();
	void StartAudio();
	void StopAudio();
	void Log(const char *format, ...);

	static void *Alloc(void *arg, uint32_t alignment, uint32_t size);
	static void Free(void *arg, void *ptr);
	static void *GpuAlloc(void *arg, uint32_t alignment, uint32_t size);
	static void GpuFree(void *arg, void *ptr);
	static void *CpuFrameAlloc(void *arg, uint32_t alignment, uint32_t size);
	static void CpuFrameFree(void *arg, void *ptr);
	static int FileOpen(void *arg, const char *filename);
	static int FileClose(void *arg);
	static int FileRead(void *arg, uint8_t *buffer, uint64_t position, uint32_t length);
	static uint64_t FileSize(void *arg);
	static int AudioThread(SceSize args, void *argp);

	paf::string m_path;
	SceAvPlayerHandle m_player;
	int32_t m_mode;
	int32_t m_errors[3];
	uint64_t m_startedUs;
	uint64_t m_duration;
	bool m_haveFrame;
	bool m_paused;
	bool m_failed;
	int32_t m_width;
	int32_t m_height;
	paf::vector<SurfaceEntry> m_surfaces;
	paf::intrusive_ptr<paf::graph::Surface> m_frame;
	paf::intrusive_ptr<paf::graph::Surface> m_cpuSurface;
	uint32_t *m_cpuPixels;
	SceUID m_audioThread;
	volatile bool m_audioRun;
	SceUID m_fileFd;
	uint64_t m_fileSize;

	static VideoPlayer *s_active;
};

#endif
