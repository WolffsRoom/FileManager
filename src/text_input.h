#ifndef _FM_TEXT_INPUT_H_
#define _FM_TEXT_INPUT_H_

#include <sdk_compat.h>
#include <paf.h>

/**
 * Modal page with a system text box (opens the system keyboard). Deletes itself
 * after calling the listener; `text` is empty when the user cancelled.
 */
class TextInput
{
public:

	typedef void (*Callback)(const paf::wstring &text, void *userdata);

	TextInput(paf::ui::Widget *parentRoot, const wchar_t *title, const paf::wstring &initial, Callback cb, void *userdata);

	~TextInput();

private:

	void Finish(bool accepted);

	paf::ui::Widget *m_parentRoot;
	paf::ui::Scene *m_root;
	paf::ui::TextBox *m_box;
	Callback m_cb;
	void *m_userdata;
	paf::wstring m_result;
};

#endif
