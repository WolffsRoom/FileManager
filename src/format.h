#ifndef _FM_FORMAT_H_
#define _FM_FORMAT_H_

#include <sdk_compat.h>
#include <paf.h>

namespace format {

	/** "512 B", "1.5 KB", "12.3 MB", "4.0 GB" */
	paf::string Size(uint64_t bytes);
}

#endif
