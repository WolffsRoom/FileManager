#ifndef _FM_SETTINGS_H_
#define _FM_SETTINGS_H_

#include <sdk_compat.h>
#include <paf.h>

#define FM_DATA_DIR "ux0:data/FileManager"
#define FM_BACKGROUND_PATH FM_DATA_DIR "/background.png"
#define FM_BACKGROUNDS_DIR FM_DATA_DIR "/backgrounds"

namespace settings {

	enum ViewMode
	{
		View_List = 0,
		View_TwoColumns = 1,
		View_Count
	};

	struct Config
	{
		int32_t theme;
		int32_t view;
		int32_t language;        // system, Português (Brasil), English, Español, Français, Deutsch
		int32_t selectFunction;  // 0 FTP server, 1 QR code, 2 USB
		int32_t usbDevice;       // -1 asks first; 0 memory, 1 game card, 2 SD2Vita, 3 PSVSD
		int32_t fontSize;        // 0 small, 1 normal (original), 2 large
		int32_t autoDecrypt;     // mount protected application data automatically
		int32_t sortMode;        // 0 name, 1 date, 2 type
	};

	float FileFontSize();

	void Load();

	/** Applies the language setting to the plugin's strings. */
	void ApplyLanguage();
	void Save();
	Config &Get();
}

namespace theme {

	struct Info
	{
		uint32_t nameHash;
		float bg[3];     // multiplies the neutral background
		float accent[3]; // storage bars and title text
	};

	uint32_t Count();
	const Info &At(uint32_t index);

	/** Index of the theme that uses FM_BACKGROUND_PATH. */
	uint32_t CustomIndex();

	/** Applies the current theme to a full-screen background plane. */
	void ApplyBackground(paf::ui::Widget *plane);
	/** Advances custom backgrounds and fades the next image in. */
	void UpdateBackground(paf::ui::Widget *plane);

	const float *Accent();
}

#endif
