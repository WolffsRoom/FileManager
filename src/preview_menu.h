#ifndef _FM_PREVIEW_MENU_H_
#define _FM_PREVIEW_MENU_H_

#include <sdk_compat.h>
#include <paf.h>

/**
 * Full-screen preview of images and MP4 videos. L/R go to the previous and next
 * previewable file of the folder, Cross pauses a video, Circle closes.
 * Videos have no sound here: this is a preview, the player is still to be ported.
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

	/** Closes the page on the next frame. */
	void Close();

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
	bool m_paused;
	bool m_frameShown;
	uint32_t *m_rgba;
	int32_t m_videoW;
	int32_t m_videoH;
	paf::intrusive_ptr<paf::graph::Surface> m_videoSurf;
};

#endif
