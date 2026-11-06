#ifndef _FM_PREVIEW_MENU_H_
#define _FM_PREVIEW_MENU_H_

#include <sdk_compat.h>
#include <paf.h>

/**
 * Full-screen preview of images and MP4 videos. L/R go to the previous and next
 * previewable file of the folder, Cross pauses a video, Circle closes. Images
 * support swipe navigation, pinch zoom and one-finger panning while enlarged.
 */
class PreviewMenu
{
public:

	PreviewMenu(paf::ui::Widget *parentRoot, const paf::vector<paf::string> &paths, int32_t index);
	~PreviewMenu();

	/** Worker-thread side of loading an image. */
	void LoadImageJob();

	/** Runs on the main thread, once per frame, while a video is open. */
	void OnVideoFrame();
	void OnImageTouch();

	/** Closes the page on the next frame. */
	void Close();

	void SeekVideo(int32_t deltaMs);

private:

	void Open(int32_t index);
	void StopVideo();
	bool StartVideo(const char *path);
	void ShowError();

	paf::ui::Widget *m_parentRoot;
	paf::ui::Scene *m_root;
	paf::ui::Widget *m_plane;
	paf::ui::BusyIndicator *m_busy;
	paf::ui::Text *m_title;
	paf::ui::Text *m_error;

	paf::vector<paf::string> m_paths;
	int32_t m_index;
	volatile int32_t m_loadId;
	int32_t m_jobId;
	paf::string m_jobPath;

	int32_t m_player;
	bool m_videoActive;
	bool m_imageActive;
	bool m_paused;
	bool m_frameShown;
	uint32_t *m_rgba;
	int32_t m_videoW;
	int32_t m_videoH;
	paf::intrusive_ptr<paf::graph::Surface> m_videoSurf;
	paf::ui::Widget *m_controls;
	paf::ui::Widget *m_time;
	paf::ui::Widget *m_barFill;
	uint64_t m_duration;
	uint32_t m_frameCounter;
	bool m_touchActive;
	bool m_pinchActive;
	float m_touchStartX;
	float m_touchStartY;
	float m_touchLastX;
	float m_touchLastY;
	float m_pinchStart;
	float m_pinchScale;
	float m_imageScale;
	float m_imageX;
	float m_imageY;
};

#endif
