#ifndef _FM_MUSIC_MENU_H_
#define _FM_MUSIC_MENU_H_

#include <sdk_compat.h>
#include <paf.h>

/**
 * Music player in the style of the Vita's Music app: album art on the left, song,
 * artist and album on the right, progress and controls below. Plays the folder's
 * MP3/OGG files in order. Cross play/pause, L/R previous/next, Left/Right -10/+10 s.
 */
class MusicMenu
{
public:

	MusicMenu(paf::ui::Widget *parentRoot, const paf::vector<paf::string> &paths, int32_t index);
	~MusicMenu();

	void Update();
	void Play(int32_t index);
	void Step(int32_t delta);
	void Seek(int32_t seconds);
	void TogglePause();
	void ExportToMusic();
	void ListenInBackground();
	void OpenInMusic();
	void Close();
	static bool HasMinimized();
	static void RestoreMinimized();

private:

	void ShowCover(const char *path, int32_t type, int32_t offset, int32_t length);
	void UpdateSeekPreview(float percent);
	int32_t ExportCurrent(bool notify);

	paf::ui::Widget *m_parentRoot;
	paf::ui::Scene *m_root;
	paf::ui::Widget *m_cover;
	paf::ui::Widget *m_title;
	paf::ui::Widget *m_artist;
	paf::ui::Widget *m_album;
	paf::ui::Widget *m_elapsed;
	paf::ui::Widget *m_total;
	paf::ui::Widget *m_barFill;
	paf::ui::Widget *m_seekMarker;
	paf::ui::Widget *m_playButton;
	paf::ui::Widget *m_playGlyph;
	paf::ui::Widget *m_pauseGlyph;
	paf::vector<paf::string> m_paths;
	int32_t m_index;
	long m_seconds;
	uint32_t m_frame;
	bool m_playing;
	bool m_closing;
	bool m_barTouch;
	float m_barTouchX;
	bool m_minimized;
};

#endif
