#include <sdk_compat.h>
#include <paf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "common.h"
#include "settings.h"

using namespace paf;

namespace {

	settings::Config s_config = { 0, settings::View_List, 0, 0, -1, 1, 1, 0 };

	const theme::Info k_themes[] = {
		{ msg_theme_default,  { 1.00f, 1.00f, 1.00f }, { 1.00f, 1.00f, 1.00f } },
		{ msg_theme_midnight, { 0.10f, 0.10f, 0.13f }, { 0.55f, 0.75f, 1.00f } },
		{ msg_theme_graphite, { 0.34f, 0.36f, 0.40f }, { 0.85f, 0.88f, 0.92f } },
		{ msg_theme_ocean,    { 0.05f, 0.42f, 0.55f }, { 0.45f, 0.95f, 1.00f } },
		{ msg_theme_purple,   { 0.33f, 0.16f, 0.58f }, { 0.82f, 0.68f, 1.00f } },
		{ msg_theme_emerald,  { 0.05f, 0.40f, 0.27f }, { 0.55f, 1.00f, 0.72f } },
		{ msg_theme_wine,     { 0.46f, 0.08f, 0.18f }, { 1.00f, 0.62f, 0.68f } },
		{ msg_theme_pink,     { 0.68f, 0.22f, 0.48f }, { 1.00f, 0.75f, 0.88f } },
		{ msg_theme_custom,   { 1.00f, 1.00f, 1.00f }, { 1.00f, 1.00f, 1.00f } },
	};
	const uint32_t k_themeCount = sizeof(k_themes) / sizeof(k_themes[0]);
	paf::vector<paf::string> s_backgrounds;
	uint32_t s_backgroundIndex = 0;
	uint64_t s_backgroundTick = 0;

	void FindBackgrounds()
	{
		s_backgrounds.clear();
		SceIoStat stat;
		if (sceIoGetstat(FM_BACKGROUND_PATH, &stat) >= 0)
			s_backgrounds.push_back(FM_BACKGROUND_PATH);
		SceUID dir = sceIoDopen(FM_BACKGROUNDS_DIR);
		if (dir >= 0)
		{
			SceIoDirent ent;
			while (memset(&ent, 0, sizeof(ent)), sceIoDread(dir, &ent) > 0)
			{
				const char *dot = strrchr(ent.d_name, '.');
				if (!SCE_S_ISDIR(ent.d_stat.st_mode) && dot && !strcasecmp(dot, ".png"))
					s_backgrounds.push_back(paf::string(FM_BACKGROUNDS_DIR "/") + ent.d_name);
			}
			sceIoDclose(dir);
		}
		s_backgroundIndex = 0;
		s_backgroundTick = sceKernelGetProcessTimeWide();
	}

	intrusive_ptr<graph::Surface> LoadBackground(const paf::string &path)
	{
		int32_t res = -1;
		auto file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &res);
		return res == 0 ? graph::Surface::Load(gutil::GetDefaultSurfacePool(), (common::SharedPtr<File> &)file) : intrusive_ptr<graph::Surface>();
	}
}

void settings::Load()
{
	sceIoMkdir(FM_DATA_DIR, 0777);
	sceIoRemove(FM_DATA_DIR "/perf.log"); // holds the timings of this session only

	SceUID fd = sceIoOpen(FM_DATA_DIR "/config.txt", SCE_O_RDONLY, 0);
	if (fd < 0)
	{
		return;
	}

	char buf[128];
	int32_t size = sceIoRead(fd, buf, sizeof(buf) - 1);
	sceIoClose(fd);
	if (size <= 0)
	{
		return;
	}
	buf[size] = 0;

	const char *p = strstr(buf, "theme=");
	if (p)
	{
		int32_t v = atoi(p + 6);
		if (v >= 0 && v < (int32_t)k_themeCount)
		{
			s_config.theme = v;
		}
	}
	p = strstr(buf, "language=");
	if (p)
	{
		int32_t v = atoi(p + 9);
		if (v >= 0 && v <= 5)
		{
			s_config.language = v;
		}
	}
	p = strstr(buf, "select=");
	if (p)
	{
		int32_t v = atoi(p + 7);
		if (v >= 0 && v <= 2)
		{
			s_config.selectFunction = v;
		}
	}
	p = strstr(buf, "usb_device=");
	if (p)
	{
		int32_t v = atoi(p + 11);
		if (v >= -1 && v <= 3) s_config.usbDevice = v;
	}
	p = strstr(buf, "view=");
	if (p)
	{
		int32_t v = atoi(p + 5);
		if (v >= 0 && v < View_Count)
		{
			s_config.view = v;
		}
	}
	p = strstr(buf, "font=");
	if (p)
	{
		int32_t v = atoi(p + 5);
		if (v >= 0 && v <= 2)
		{
			s_config.fontSize = v;
		}
	}
	p = strstr(buf, "auto_decrypt=");
	if (p)
	{
		int32_t v = atoi(p + 13);
		if (v >= 0 && v <= 1)
		{
			s_config.autoDecrypt = v;
		}
	}
	p = strstr(buf, "sort=");
	if (p)
	{
		int32_t v = atoi(p + 5);
		if (v >= 0 && v <= 2) s_config.sortMode = v;
	}
}

void settings::Save()
{
	SceUID fd = sceIoOpen(FM_DATA_DIR "/config.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
	if (fd < 0)
	{
		return;
	}
	char buf[160];
	int32_t n = snprintf(buf, sizeof(buf), "theme=%d\nview=%d\nlanguage=%d\nselect=%d\nusb_device=%d\nfont=%d\nauto_decrypt=%d\nsort=%d\n",
		(int)s_config.theme, (int)s_config.view, (int)s_config.language, (int)s_config.selectFunction,
		(int)s_config.usbDevice, (int)s_config.fontSize, (int)s_config.autoDecrypt, (int)s_config.sortMode);
	sceIoWrite(fd, buf, n);
	sceIoClose(fd);
}

void settings::ApplyLanguage()
{
	// "pt" carries the Brazilian strings in this resource.
	static const Locale k_locales[] = { Locale_Default, Locale_PT, Locale_EN, Locale_ES, Locale_FR, Locale_DE };
	Locale locale = k_locales[s_config.language];
	if (locale == Locale_Default)
	{
		locale = Framework::Instance()->GetLocale();
	}
	g_appPlugin->SetLocale(locale);
}

settings::Config &settings::Get()
{
	return s_config;
}

float settings::FileFontSize()
{
	static const float k_sizes[] = { 22.0f, 28.0f, 34.0f };
	int32_t index = s_config.fontSize;
	return k_sizes[index >= 0 && index <= 2 ? index : 1];
}

uint32_t theme::Count()
{
	return k_themeCount;
}

const theme::Info &theme::At(uint32_t index)
{
	return k_themes[index < k_themeCount ? index : 0];
}

uint32_t theme::CustomIndex()
{
	return k_themeCount - 1;
}

const float *theme::Accent()
{
	return At(settings::Get().theme).accent;
}

void theme::ApplyBackground(ui::Widget *plane)
{
	const uint32_t index = settings::Get().theme;
	const Info &info = At(index);

	intrusive_ptr<graph::Surface> tex;
	if (index == CustomIndex())
	{
		FindBackgrounds();
		if (!s_backgrounds.empty()) tex = LoadBackground(s_backgrounds.at(0));
	}
	if (!tex.get())
	{
		// Without a usable image the custom theme falls back to the default look.
		tex = g_appPlugin->GetTexture(index == 0 || index == CustomIndex() ? tex_common_bg : tex_bg_neutral);
	}

	if (tex.get())
	{
		plane->SetTexture(tex, 0, 0);
	}
	plane->SetColor(info.bg[0], info.bg[1], info.bg[2], 1.0f);
}

void theme::UpdateBackground(ui::Widget *plane)
{
	if (settings::Get().theme != CustomIndex() || s_backgrounds.size() < 2 || !plane)
		return;
	uint64_t now = sceKernelGetProcessTimeWide();
	if (now - s_backgroundTick < 8000000ULL)
		return;
	s_backgroundTick = now;
	s_backgroundIndex = (s_backgroundIndex + 1) % s_backgrounds.size();
	intrusive_ptr<graph::Surface> tex = LoadBackground(s_backgrounds.at(s_backgroundIndex));
	if (!tex.get()) return;
	plane->SetTexture(tex, 0, 0);
	plane->SetColor(1.0f, 1.0f, 1.0f, 0.0f);
	plane->SetColor(1.0f, 1.0f, 1.0f, 1.0f, new Timer(0.8f, Timer::FUNC_SINE_OUT));
}
