#include "format.h"

using namespace paf;

paf::string format::Size(uint64_t bytes)
{
	static const char *const units[] = { "B", "KB", "MB", "GB", "TB" };

	if (bytes < 1024)
	{
		return common::FormatString("%u B", (uint32_t)bytes);
	}

	int32_t unit = 0;
	uint64_t scale = 1;
	while (bytes / scale >= 1024 && unit < 4)
	{
		scale *= 1024;
		unit++;
	}

	// One decimal, computed in integers so the output is exact and needs no float printf.
	uint64_t tenths = (bytes * 10 + scale / 2) / scale;
	return common::FormatString("%u.%u %s", (uint32_t)(tenths / 10), (uint32_t)(tenths % 10), units[unit]);
}
