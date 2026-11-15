#ifndef _FM_VPK_ASSETS_H_
#define _FM_VPK_ASSETS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VpkAssetReport {
	uint32_t compatible;
	uint32_t fixable;
	uint32_t invalid;
	char firstInvalid[256];
} VpkAssetReport;

/** Inspects presentation PNGs inside a VPK without extracting it. */
int inspectVpkAssets(const char *vpk, VpkAssetReport *report);

/** Converts incompatible presentation PNGs below an extracted VPK root. */
int normalizeExtractedVpkAssets(const char *root, VpkAssetReport *report);

#ifdef __cplusplus
}
#endif

#endif
