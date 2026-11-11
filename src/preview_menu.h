#ifndef _FM_PREVIEW_MENU_H_
#define _FM_PREVIEW_MENU_H_

#include <sdk_compat.h>
#include <paf.h>

/**
 * Full-screen preview of images and MP4 videos. L/R go to the previous and next
 * previewable file of the folder, Cross pauses a video, Circle closes. Images
 * support swipe navigation, pinch zoom, mirroring and vertical inversion.
 */
class PreviewMenu
{
public:

	typedef void (*ChangeCallback)(void *user);
	PreviewMenu(paf::ui::Widget *parentRoot, const paf::vector<paf::string> &paths,
		int32_t index, ChangeCallback onChanged = NULL, void *userdata = NULL);
	~PreviewMenu();

	/** Worker-thread side of loading an image. */
	void LoadImageJob();

	/** Runs on the main thread, once per frame, while a video is open. */
	void OnVideoFrame();
	void OnVideoSeek();
	void OnThumbnail(int32_t delta);
	void OnImageTouch();
	void OnOptions();
	void OnOption(int32_t index);
	void OnRenameEntered(const paf::wstring &text);
	void OnDeleteAnswer(int32_t buttonCode);
	void SetArrowPressed(bool left, bool pressed);

	/** Closes the page on the next frame. */
	void Close();

	void SeekVideo(int32_t deltaMs);
	void SetViewMode(int32_t mode);
	void StepImage(int32_t delta);

private:
	struct VideoSurfaceEntry
	{
		VideoSurfaceEntry() : data(NULL) {}
		void *data;
		paf::intrusive_ptr<paf::graph::Surface> surface;
	};

	void Open(int32_t index);
	void StopVideo();
	bool StartVideo(const char *path);
	void ShowError();
	void ApplyImageTransform();
	void RemoveCurrentPath();
	enum { OptionTag = 0x50525657 };

	paf::ui::Widget *m_parentRoot;
	ChangeCallback m_onChanged;
	void *m_userdata;
	bool m_filesChanged;
	paf::ui::Scene *m_root;
	paf::ui::Widget *m_background;
	paf::ui::Widget *m_plane;
	paf::ui::BusyIndicator *m_busy;
	paf::ui::Text *m_title;
	paf::ui::Text *m_error;
	paf::ui::Widget *m_leftArrow;
	paf::ui::Widget *m_rightArrow;
	paf::ui::Widget *m_thumbLeft;
	paf::ui::Widget *m_thumbCurrent;
	paf::ui::Widget *m_thumbRight;
	paf::ui::Widget *m_thumbLeftButton;
	paf::ui::Widget *m_thumbCurrentButton;
	paf::ui::Widget *m_thumbRightButton;

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
	int32_t m_videoW;
	int32_t m_videoH;
	paf::vector<VideoSurfaceEntry> m_videoSurfaces;
	paf::intrusive_ptr<paf::graph::Surface> m_currentSurf;
	paf::intrusive_ptr<paf::graph::Surface> m_leftSurf;
	paf::intrusive_ptr<paf::graph::Surface> m_rightSurf;
	paf::ui::Widget *m_controls;
	paf::ui::Widget *m_time;
	paf::ui::ProgressBarTouch *m_videoSlider;
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
	float m_imageInitialScale;
	float m_imageX;
	float m_imageY;
	bool m_flipX;
	bool m_flipY;
	int32_t m_viewMode;
	bool m_deletePending;
	bool m_animateNextImage;
};

#endif
