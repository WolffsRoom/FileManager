#ifndef _FM_VIDEO_PLAYER_H_
#define _FM_VIDEO_PLAYER_H_
#include <sdk_compat.h>
#include <paf.h>
#include <psp2/avplayer.h>
bool AvPlayerHandleValid(SceAvPlayerHandle handle);
class VideoPlayer {
public:
	VideoPlayer(); ~VideoPlayer();
	bool Open(const char *path); void Close(); void Update(); void PauseResume();
	void Seek(int32_t deltaMs); void SeekTo(uint64_t ms); uint64_t CurrentTime() const;
	uint64_t Duration() const { return m_duration; }
	paf::intrusive_ptr<paf::graph::Surface> Frame() const { return m_frame; }
	bool HasFirstFrame() const { return m_haveFrame; }
	bool IsPaused() const { return m_paused; }
	int SetSpeed(int32_t percent);
	int32_t Speed() const { return m_speed; }
	bool Failed() const { return m_failed; }
private:
	struct SurfaceEntry { SurfaceEntry() : data(NULL), width(0), height(0), pitch(0) {} void *data; uint32_t width, height, pitch; paf::intrusive_ptr<paf::graph::Surface> surface; };
	bool OpenDecoder(); void CloseDecoder(); void RefreshStreams(); void StartAudio(); void StopAudio(); void Log(const char *format, ...);
	static void *Alloc(void *, uint32_t, uint32_t); static void Free(void *, void *);
	static void *GpuAlloc(void *, uint32_t, uint32_t); static void GpuFree(void *, void *);
	static int FileOpen(void *, const char *); static int FileClose(void *); static int FileRead(void *, uint8_t *, uint64_t, uint32_t); static uint64_t FileSize(void *);
	static void PlayerEvent(void *, int32_t, int32_t, void *); static int AudioThread(SceSize, void *);
	paf::string m_path; SceAvPlayerHandle m_player; uint64_t m_startedUs, m_duration;
	bool m_haveFrame, m_paused, m_failed; int32_t m_speed; volatile bool m_ready, m_started, m_eos;
	paf::vector<SurfaceEntry> m_surfaces; paf::intrusive_ptr<paf::graph::Surface> m_frame;
	SceUID m_audioThread; volatile bool m_audioRun; SceUID m_fileFd; uint64_t m_fileSize;
	static VideoPlayer *s_active;
};
#endif
