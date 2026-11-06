#ifndef _FM_NOTICE_MENU_H_
#define _FM_NOTICE_MENU_H_

#include <sdk_compat.h>
#include <paf.h>

/**
 * Panel with a title, a message and one or two buttons, in the app's own style.
 * Used where the system's error dialog is not wanted (no Wi-Fi for the FTP server,
 * with a button to the network settings).
 */
class NoticeMenu
{
public:

	/** choice: 0 primary button, 1 secondary button (or Circle). */
	typedef void (*ResultCallback)(int32_t choice, void *userdata);

	NoticeMenu(paf::ui::Widget *parentRoot, uint32_t title, uint32_t message, uint32_t primary, uint32_t secondary, ResultCallback cb, void *userdata);
	~NoticeMenu();

private:

	void Close(int32_t choice);

	paf::ui::Widget *m_parentRoot;
	paf::ui::Scene *m_root;
	ResultCallback m_cb;
	void *m_userdata;
	int32_t m_choice;
};

/** About this application. */
class AboutMenu
{
public:

	AboutMenu(paf::ui::Widget *parentRoot);
	~AboutMenu();

private:

	paf::ui::Widget *m_parentRoot;
	paf::ui::Scene *m_root;
};

#endif
