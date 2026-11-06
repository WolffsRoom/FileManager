#include <sdk_compat.h>
#include <paf.h>
#include <string.h>
#include <strings.h>
#include <psp2/io/devctl.h>

#include "common.h"
#include "core/fs.h"
#include "core/psarc.h"
#include "settings.h"
extern "C" {
#include "legacy/strnatcmp.h"
}

using namespace paf;

namespace {

	struct DeviceInfo
	{
		const char *name;
		uint32_t label;
		uint32_t icon;
		bool system;
	};

	const DeviceInfo k_devices[] = {
		{ "ux0:", msg_dev_ux0, tex_ui_root, false },
		{ "uma0:", msg_dev_uma0, tex_ui_root, false },
		{ "imc0:", msg_dev_imc0, tex_ui_root, false },
		{ "xmc0:", msg_dev_xmc0, tex_ui_root, false },
		{ "ur0:", msg_dev_ur0, tex_ui_root, false },
		{ "gro0:", msg_dev_gro0, tex_ui_root, false },
		{ "grw0:", msg_dev_grw0, tex_ui_root, false },
		{ "vs0:", msg_dev_system, tex_ui_encrypted, true },
		{ "os0:", msg_dev_system, tex_ui_encrypted, true },
		{ "sa0:", msg_dev_system, tex_ui_encrypted, true },
		{ "tm0:", msg_dev_system, tex_ui_encrypted, true },
		{ "ud0:", msg_dev_system, tex_ui_encrypted, true },
		{ "pd0:", msg_dev_system, tex_ui_encrypted, true },
		{ "sd0:", msg_dev_system, tex_ui_encrypted, true },
		{ "vd0:", msg_dev_system, tex_ui_encrypted, true },
		{ "host0:", msg_dev_system, tex_ui_encrypted, true },
	};

	struct Shortcut
	{
		const char *path;
		uint32_t label;
	};

	const Shortcut k_shortcuts[] = {
		{ "ux0:Downloads", msg_shortcut_downloads },
		{ "ux0:picture", msg_shortcut_pictures },
		{ "ux0:music", msg_shortcut_music },
		{ "ux0:pspemu", msg_shortcut_psp },
		{ "ux0:temp", msg_shortcut_trash },
		{ "ux0:user/00/savedata", msg_shortcut_saves },
		{ "ux0:app", msg_shortcut_apps },
		{ "ux0:data", msg_shortcut_data },
		{ "ur0:tai", msg_shortcut_plugins },
	};
	const uint32_t k_shortcutIcons[] = {
		tex_ui_download, tex_ui_image, tex_ui_music, tex_new_open_folder, tex_new_trash, tex_new_saves,
		tex_new_open_folder, tex_new_open_folder, tex_ui_sfo,
	};

	struct ExtKind
	{
		const char *ext;
		fs::Kind kind;
	};

	const ExtKind k_exts[] = {
		{ ".png", fs::Kind_Image }, { ".jpg", fs::Kind_Image }, { ".jpeg", fs::Kind_Image },
		{ ".bmp", fs::Kind_Image }, { ".gif", fs::Kind_Image },
		{ ".mp3", fs::Kind_Music }, { ".ogg", fs::Kind_Music }, { ".wav", fs::Kind_Music },
		{ ".at9", fs::Kind_Music }, { ".flac", fs::Kind_Music },
		{ ".mp4", fs::Kind_Video }, { ".mkv", fs::Kind_Video }, { ".avi", fs::Kind_Video },
		{ ".txt", fs::Kind_Text }, { ".ini", fs::Kind_Text }, { ".xml", fs::Kind_Text },
		{ ".cfg", fs::Kind_Text }, { ".yml", fs::Kind_Text }, { ".csv", fs::Kind_Text }, { ".lua", fs::Kind_Text }, { ".log", fs::Kind_Text }, { ".json", fs::Kind_Text },
		{ ".vpk", fs::Kind_Vpk }, { ".sfo", fs::Kind_Sfo }, { ".md", fs::Kind_Text }, { ".zip", fs::Kind_Package }, { ".rar", fs::Kind_Package },
		{ ".7z", fs::Kind_Package }, { ".tar", fs::Kind_Package }, { ".gz", fs::Kind_Package },
		{ ".psarc", fs::Kind_Package },
		{ ".suprx", fs::Kind_Plugin }, { ".skprx", fs::Kind_Plugin }, { ".self", fs::Kind_Plugin },
		{ ".bin", fs::Kind_Plugin },
	};

	fs::Kind KindForName(const char *name)
	{
		const char *dot = strrchr(name, '.');
		if (dot)
		{
			for (uint32_t i = 0; i < sizeof(k_exts) / sizeof(k_exts[0]); i++)
			{
				if (strcasecmp(dot, k_exts[i].ext) == 0)
				{
					return k_exts[i].kind;
				}
			}
		}
		return fs::Kind_Other;
	}

	bool EntryBefore(const fs::Entry *a, const fs::Entry *b)
	{
		if (a->isDir != b->isDir)
		{
			return a->isDir;
		}
		int mode = settings::Get().sortMode;
		if (mode == 1 && a->modified != b->modified)
			return a->modified > b->modified;
		if (mode == 2 && a->kind != b->kind)
			return a->kind < b->kind;
		return strnatcasecmp(a->name.c_str(), b->name.c_str()) < 0;
	}

	// Shell sort: directories can hold thousands of entries and this runs on a
	// bounded worker stack, so no recursion and no extra allocation.
	void Sort(fs::EntryList *list)
	{
		int32_t n = list->size();
		for (int32_t gap = n / 2; gap > 0; gap /= 2)
		{
			for (int32_t i = gap; i < n; i++)
			{
				fs::Entry *tmp = list->at(i);
				int32_t j = i;
				while (j >= gap && EntryBefore(tmp, list->at(j - gap)))
				{
					list->at(j) = list->at(j - gap);
					j -= gap;
				}
				list->at(j) = tmp;
			}
		}
	}

	int AddPsarcEntry(const char *name, int isDir, uint64_t size, void *userdata)
	{
		fs::EntryList *out = static_cast<fs::EntryList *>(userdata);
		fs::Entry *e = new fs::Entry();
		e->name = name;
		e->isDir = isDir != 0;
		e->kind = e->isDir ? fs::Kind_Folder : KindForName(name);
		e->marked = false;
		e->size = size;
		e->capacity = 0;
		e->modified = 0;
		e->labelHash = 0;
		e->iconHash = 0;
		e->target.clear();
		e->isShortcut = false;
		out->push_back(e);
		return 0;
	}
}

const char *const fs::k_systemPath = "::system";
const char *const fs::k_ftpPath = "::ftp";

namespace {
	paf::vector<paf::string> s_shortcuts;
	bool s_shortcutsLoaded = false;
	const char *const k_shortcutsFile = "ux0:data/FileManager/shortcuts.txt";

	void SaveShortcuts()
	{
		SceUID fd = sceIoOpen(k_shortcutsFile, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
		if (fd < 0)
		{
			return;
		}
		for (uint32_t i = 0; i < s_shortcuts.size(); i++)
		{
			sceIoWrite(fd, s_shortcuts.at(i).c_str(), s_shortcuts.at(i).length());
			sceIoWrite(fd, "\n", 1);
		}
		sceIoClose(fd);
	}
}

const paf::vector<paf::string> &shortcuts::List()
{
	if (!s_shortcutsLoaded)
	{
		s_shortcutsLoaded = true;
		SceUID fd = sceIoOpen(k_shortcutsFile, SCE_O_RDONLY, 0);
		if (fd >= 0)
		{
			static char buf[4096];
			int32_t n = sceIoRead(fd, buf, sizeof(buf) - 1);
			sceIoClose(fd);
			if (n > 0)
			{
				buf[n] = 0;
				char *line = buf;
				while (*line)
				{
					char *end = strchr(line, '\n');
					if (end)
					{
						*end = 0;
					}
					if (line[0])
					{
						s_shortcuts.push_back(paf::string(line));
					}
					if (!end)
					{
						break;
					}
					line = end + 1;
				}
			}
		}
	}
	return s_shortcuts;
}

void shortcuts::Add(const paf::string &path)
{
	List();
	for (uint32_t i = 0; i < s_shortcuts.size(); i++)
	{
		if (strcmp(s_shortcuts.at(i).c_str(), path.c_str()) == 0)
		{
			return;
		}
	}
	s_shortcuts.push_back(path);
	SaveShortcuts();
}

void shortcuts::Remove(const paf::string &path)
{
	List();
	for (uint32_t i = 0; i < s_shortcuts.size(); i++)
	{
		if (strcmp(s_shortcuts.at(i).c_str(), path.c_str()) == 0)
		{
			s_shortcuts.erase(s_shortcuts.begin() + i);
			break;
		}
	}
	SaveShortcuts();
}

bool fs::IsRootStyle(const paf::string &path)
{
	return path.empty() || strcmp(path.c_str(), k_systemPath) == 0;
}

static void AddDevices(fs::EntryList *out, bool system)
{
	for (uint32_t i = 0; i < sizeof(k_devices) / sizeof(k_devices[0]); i++)
	{
		if (k_devices[i].system != system)
		{
			continue;
		}

		SceIoStat stat;
		sce_paf_memset(&stat, 0, sizeof(stat));
		if (sceIoGetstat(k_devices[i].name, &stat) < 0)
		{
			continue;
		}

		fs::Entry *e = new fs::Entry();
		e->name = k_devices[i].name;
		e->target = k_devices[i].name;
		e->kind = fs::Kind_Device;
		e->isDir = true;
		e->marked = false;
		e->labelHash = k_devices[i].label;
		e->iconHash = k_devices[i].icon;
		e->isShortcut = false;
		e->size = 0;
		e->capacity = 0;
		e->modified = 0;

		SceIoDevInfo info;
		sce_paf_memset(&info, 0, sizeof(info));
		if (sceIoDevctl(k_devices[i].name, 0x3001, NULL, 0, &info, sizeof(info)) >= 0)
		{
			e->size = info.free_size;
			e->capacity = info.max_size;
		}
		else if (strcmp(k_devices[i].name, "ux0:") == 0)
		{
			sceAppMgrGetDevInfo("ux0:", &e->capacity, &e->size);
		}

		out->push_back(e);
	}
}

static fs::Entry *NewRootEntry(const char *name, const char *target, uint32_t label, uint32_t icon, bool isDir)
{
	fs::Entry *e = new fs::Entry();
	e->name = name;
	e->target = target;
	e->kind = fs::Kind_Folder;
	e->isDir = isDir;
	e->marked = false;
	e->labelHash = label;
	e->iconHash = icon;
	e->size = 0;
	e->capacity = 0;
	e->modified = 0;
	e->isShortcut = false;
	return e;
}

void fs::ListRoot(EntryList *out)
{
	AddDevices(out, false);

	const paf::vector<paf::string> &user = shortcuts::List();
	for (uint32_t i = 0; i < user.size(); i++)
	{
		Entry *e = NewRootEntry(user.at(i).c_str(), user.at(i).c_str(), 0, tex_ui_bookmark, true);
		e->isShortcut = true;
		out->push_back(e);
	}

	for (uint32_t i = 0; i < sizeof(k_shortcuts) / sizeof(k_shortcuts[0]); i++)
	{
		SceIoStat stat;
		if (sceIoGetstat(k_shortcuts[i].path, &stat) < 0)
		{
			continue;
		}
		Entry *e = new Entry();
		e->name = k_shortcuts[i].path;
		e->target = k_shortcuts[i].path;
		e->kind = Kind_Folder;
		e->isDir = true;
		e->marked = false;
		e->labelHash = k_shortcuts[i].label;
		e->iconHash = k_shortcutIcons[i];
		e->isShortcut = false;
		e->size = 0;
		e->capacity = 0;
		e->modified = 0;
		out->push_back(e);
	}


	Entry *e = new Entry();
	e->isShortcut = false;
	e->name = "vs0: os0: sa0: tm0: ud0: pd0:";
	e->target = k_systemPath;
	e->kind = Kind_Folder;
	e->isDir = true;
	e->marked = false;
	e->labelHash = msg_system_partitions;
	e->iconHash = tex_ui_encrypted;
	e->size = 0;
	e->capacity = 0;
	e->modified = 0;
	out->push_back(e);
}

void fs::ListSystem(EntryList *out)
{
	AddDevices(out, true);
}

int32_t fs::ListDirectory(const char *path, EntryList *out)
{
	if (psarcContainsPath(path))
	{
		int32_t res = psarcList(path, AddPsarcEntry, out);
		if (res >= 0) Sort(out);
		return res;
	}
	SceUID dfd = sceIoDopen(path);
	if (dfd < 0)
	{
		return dfd;
	}

	while (true)
	{
		SceIoDirent dir;
		sce_paf_memset(&dir, 0, sizeof(dir));
		int32_t res = sceIoDread(dfd, &dir);
		if (res <= 0)
		{
			break;
		}

		Entry *e = new Entry();
		e->name = dir.d_name;
		e->isDir = SCE_S_ISDIR(dir.d_stat.st_mode);
		e->kind = e->isDir ? Kind_Folder : KindForName(dir.d_name);
		e->marked = false;
		e->size = dir.d_stat.st_size;
		e->capacity = 0;
		e->modified = ((uint64_t)dir.d_stat.st_mtime.year << 48) |
			((uint64_t)dir.d_stat.st_mtime.month << 40) | ((uint64_t)dir.d_stat.st_mtime.day << 32) |
			((uint64_t)dir.d_stat.st_mtime.hour << 24) | ((uint64_t)dir.d_stat.st_mtime.minute << 16) |
			((uint64_t)dir.d_stat.st_mtime.second << 8);
		e->labelHash = 0;
		e->iconHash = 0;
		e->isShortcut = false;
		out->push_back(e);
	}

	sceIoDclose(dfd);
	Sort(out);
	if (!strcasecmp(path, "ur0:tai") || !strcasecmp(path, "ux0:tai"))
	{
		for (uint32_t i = 0; i < out->size(); i++)
		{
			if (!strcasecmp(out->at(i)->name.c_str(), "config.txt"))
			{
				fs::Entry *config = out->at(i);
				for (uint32_t j = i; j > 0; j--) out->at(j) = out->at(j - 1);
				out->at(0) = config;
				break;
			}
		}
	}
	return 0;
}

void fs::FreeList(EntryList *list)
{
	for (uint32_t i = 0; i < list->size(); i++)
	{
		delete list->at(i);
	}
	list->clear();
}

paf::string fs::Join(const paf::string &path, const paf::string &name)
{
	paf::string out = path;
	if (!out.empty() && out.c_str()[out.length() - 1] != ':' && out.c_str()[out.length() - 1] != '/')
	{
		out += "/";
	}
	out += name;
	return out;
}

bool fs::IsValidName(const char *name)
{
	if (!name[0] || !strcmp(name, ".") || !strcmp(name, ".."))
	{
		return false;
	}
	return strpbrk(name, "/\\:*?\"<>|") == NULL;
}

paf::string fs::UniqueName(const paf::string &dir, const paf::string &name)
{
	paf::string candidate = name;
	SceIoStat stat;
	for (int32_t i = 2; i < 1000; i++)
	{
		if (sceIoGetstat(Join(dir, candidate).c_str(), &stat) < 0)
		{
			break;
		}

		const char *base = name.c_str();
		const char *dot = strrchr(base, '.');
		if (dot && dot != base)
		{
			paf::string stem(base, dot - base);
			candidate = common::FormatString("%s (%d)%s", stem.c_str(), i, dot);
		}
		else
		{
			candidate = common::FormatString("%s (%d)", base, i);
		}
	}
	return candidate;
}
