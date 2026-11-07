#ifndef _OPTION_MENU_H_
#define _OPTION_MENU_H_

#include <sdk_compat.h>
#include <kernel.h>
#include <paf.h>

using namespace paf;

#define OPTION_MENU_BUTTON_MAGIC 0xa5448baf

class OptionMenu
{
public:

	enum
	{
		OptionMenuEvent = (ui::Handler::CB_STATE + 0x50000),
	};

	enum OptionMenuEvent
	{
		OptionMenuEvent_Button,
		OptionMenuEvent_Close
	};

	class Button
	{
	public:

		paf::wstring label;
	};

	/** Explicitly allocate before construction; operator new can return NULL on Vita. */
	static OptionMenu *Create(Plugin *workPlugin, ui::Widget *root, vector<Button> *buttons,
		uint32_t pageHash = 0xd098335e, int32_t tag = 0);

	OptionMenu(Plugin *workPlugin, ui::Widget *root, vector<Button> *buttons, uint32_t pageHash = 0xd098335e, int32_t tag = 0);

	~OptionMenu();

private:

	void OnButton(ui::Widget *wdg);
	bool m_closing;
	int32_t m_selectedHash;

	Plugin *m_plugin;
	ui::Scene *m_rootScene;
	ui::Widget *m_parentRoot;
	ui::Widget *m_optPlaneRoot;
	uint32_t m_buttonCount;
	uint32_t m_pageHash;
	int32_t m_tag;   // sent with the events, so each page reacts only to its own menu
};

#endif
