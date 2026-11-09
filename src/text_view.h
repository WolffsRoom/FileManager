#ifndef _FM_TEXT_VIEW_H_
#define _FM_TEXT_VIEW_H_

#include <sdk_compat.h>
#include <paf.h>

/**
 * Text viewer and line editor (as in VitaShell/FMVita): one list row per line.
 * Triangle opens incremental search. Start opens Edit line, Insert line, Delete
 * line and Save. Leaving with unsaved changes asks whether to save them.
 */
class TextView
{
public:

	enum { OptionTag = 2 };   // tells this page's option menu apart from the browser's

	class ListViewCb : public paf::ui::listview::ItemFactory
	{
	public:
		ListViewCb(TextView *parent) : m_parent(parent) {}
		~ListViewCb() {}
		paf::ui::ListItem *Create(CreateParam &param) { return m_parent->CreateLine(param); }
		void Start(StartParam &param) { param.list_item->Show(paf::common::transition::Type_Reset); }
	private:
		TextView *m_parent;
	};

	TextView(paf::ui::Widget *parentRoot, const char *path);
	~TextView();

	paf::ui::ListItem *CreateLine(paf::ui::listview::ItemFactory::CreateParam &param);

	void OnOption(int32_t index);
	void OnLineEntered(const paf::wstring &text);
	void JumpTo(bool end);
	void OnSaveAnswer(int32_t choice);
	void PollSearch();

private:

	void OnBack();
	void OnOptions();
	void Rebuild();
	int32_t Save();
	void RefreshRow(int32_t row);
	void SetSearchVisible(bool visible);
	void ApplySearch(bool finishEditing);
	void CloseSearch();
	int32_t OriginalRow(int32_t visibleRow) const;

	enum Edit { Edit_Change, Edit_Insert };

	paf::ui::Widget *m_parentRoot;
	paf::ui::Scene *m_root;
	paf::ui::ListView *m_list;
	paf::string m_path;
	paf::vector<paf::string> m_lines;
	paf::vector<int32_t> m_visibleLines;
	paf::ui::TextBox *m_search;
	paf::string m_lastSearchQuery;
	uint32_t m_rows;
	int32_t m_focused;
	int32_t m_editRow;
	bool m_dirty;
	bool m_crlf;        // keep the file's own line endings
	bool m_truncated;   // only the first part of a big file was loaded: never save it
	bool m_searchVisible;
	Edit m_edit;
};

#endif
