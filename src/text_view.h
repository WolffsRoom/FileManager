#ifndef _FM_TEXT_VIEW_H_
#define _FM_TEXT_VIEW_H_

#include <sdk_compat.h>
#include <paf.h>

/** Read-only text viewer: one list row per line, so big files scroll smoothly. */
class TextView
{
public:

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

private:

	void Close();

	paf::ui::Widget *m_parentRoot;
	paf::ui::Scene *m_root;
	paf::ui::ListView *m_list;
	char *m_data;                    // file contents, lines point into it
	paf::vector<uint32_t> m_lines;   // offset of each line, wrapped
};

#endif
