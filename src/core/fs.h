#ifndef _FM_FS_H_
#define _FM_FS_H_

#include <sdk_compat.h>
#include <paf.h>

namespace fs {

	enum Kind
	{
		Kind_Device,
		Kind_Folder,
		Kind_Image,
		Kind_Music,
		Kind_Video,
		Kind_Text,
		Kind_Package,
		Kind_Plugin,
		Kind_Vpk,
		Kind_Sfo,
		Kind_Game,
		Kind_Other
	};

	struct Entry
	{
		paf::string name;      // "ux0:" for a device, the file name otherwise
		Kind kind;
		bool isDir;
		bool marked;
		uint64_t size;         // file size, or free space for a device
		uint64_t capacity;     // device capacity
		uint64_t modified;     // packed modification date for sorting
		uint32_t labelHash;    // display name in the string table, 0 for plain files
		uint32_t iconHash;     // icon texture for root entries
		paf::string target;    // what opening this entry shows
		bool isShortcut;       // added by the user, can be removed
	};

	typedef paf::vector<Entry *> EntryList;

	/** Pseudo path of the page that lists the system partitions. */
	extern const char *const k_systemPath;

	/** Pseudo path of the root entry that starts the FTP server. */
	extern const char *const k_ftpPath;

	/** Storage devices, shortcuts and the entry for the system partitions. */
	void ListRoot(EntryList *out);

	/** Mounted system partitions. */
	void ListSystem(EntryList *out);

	/** Pages of devices and shortcuts, as opposed to real folders. */
	bool IsRootStyle(const paf::string &path);

	/** Folders first, then natural order by name. Returns a negative error on failure. */
	int32_t ListDirectory(const char *path, EntryList *out);

	/** True for supported game, emulator and packaging file formats. */
	bool IsGameFormat(const char *name);

	void FreeList(EntryList *list);

	/** path + "/" + name, without doubling the separator after a device ("ux0:"). */
	paf::string Join(const paf::string &path, const paf::string &name);

	/** Whether the name can be used on the Vita's file systems. */
	bool IsValidName(const char *name);

	/** "name", "name (2)", ... the first that does not exist inside dir. */
	paf::string UniqueName(const paf::string &dir, const paf::string &name);
}

#endif

// Folders the user pinned to the root page (FMVita's bookmarks).
namespace shortcuts {

	const paf::vector<paf::string> &List();
	void Add(const paf::string &path);
	void Remove(const paf::string &path);
	bool Contains(const paf::string &path);
}
