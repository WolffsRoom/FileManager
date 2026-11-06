#ifndef _FM_FTP_MENU_H_
#define _FM_FTP_MENU_H_

#include <sdk_compat.h>
#include <paf.h>

/**
 * FTP status page: address, connection, current transfer with its progress.
 * OK (or Circle) leaves the server running in the background; Cancel stops it.
 */
class FtpMenu
{
public:

	typedef void (*CloseCallback)(bool stopped, void *userdata);

	FtpMenu(paf::ui::Widget *parentRoot, CloseCallback cb, void *userdata);
	~FtpMenu();

	/** Main thread, every frame. */
	void Update();

private:

	void Close(bool stop);

	paf::ui::Widget *m_parentRoot;
	paf::ui::Scene *m_root;
	paf::ui::Widget *m_address;
	paf::ui::Widget *m_connection;
	paf::ui::Widget *m_activity;
	paf::ui::Widget *m_progressText;
	paf::ui::Widget *m_bar;
	paf::ui::Widget *m_barFill;
	CloseCallback m_cb;
	void *m_userdata;
	bool m_stopped;
	uint32_t m_frame;
	int32_t m_lastActivity;
	uint64_t m_lastDone;
};

#endif
