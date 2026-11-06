#ifndef _FM_INFO_MENU_H_
#define _FM_INFO_MENU_H_

#include <sdk_compat.h>
#include <paf.h>

#include "core/fs.h"

/**
 * Information panel for a file, and the install prompt for VPKs: a picture on the
 * left (image preview, or the app's round bubble for a VPK) and details beside it.
 */
class InfoMenu
{
public:

	enum Mode
	{
		Mode_Info,
		Mode_Install   // asks "Install this content?", Install / Cancel
	};

	typedef void (*ResultCallback)(bool accepted, void *userdata);

	InfoMenu(paf::ui::Widget *parentRoot, Mode mode, const paf::string &path, const fs::Entry *entry, ResultCallback cb, void *userdata);
	~InfoMenu();

private:

	void Close(bool accepted);
	void AddLine(const char *text);
	void ShowPicture(void *png, uint32_t size);

	paf::ui::Widget *m_parentRoot;
	paf::ui::Scene *m_root;
	paf::ui::Widget *m_picture;
	paf::string m_text;
	ResultCallback m_cb;
	void *m_userdata;
	bool m_accepted;
};

#endif
