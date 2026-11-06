#ifndef _NP_H_
#define _NP_H_

#include <stdint.h>
#include <stddef.h>
#include <psp2/types.h>
#include <psp2/np/common.h>

struct SceNpId {
	char handle[32];
	uint8_t dummy[8];
};

typedef struct SceNpCommunicationPassphrase {
	uint8_t data[128];
} SceNpCommunicationPassphrase;

typedef struct SceNpCommunicationSignature {
	uint8_t data[160];
} SceNpCommunicationSignature;

typedef int32_t SceNpTusSlotId;
typedef int32_t SceNpTusRequest;

typedef struct SceNpTusDataInfo {
	uint8_t data[64];
	uint32_t infoSize;
} SceNpTusDataInfo;

typedef struct SceNpTusDataStatus {
	struct {
		uint32_t infoSize;
		void *data;
	} info;
	int hasData;
	size_t dataSize;
} SceNpTusDataStatus;

typedef enum SceNpServiceState {
	SCE_NP_SERVICE_STATE_OFFLINE = 0,
	SCE_NP_SERVICE_STATE_ONLINE = 1
} SceNpServiceState;

typedef struct SceNpCommunicationConfig {
	const void *commId;
	const void *commPassphrase;
	const void *commSignature;
} SceNpCommunicationConfig;

static inline int sceNpInit(const void *a1 = NULL, void *a2 = NULL) { return 0; }
static inline int sceNpTerm() { return 0; }
static inline int sceNpAuthInit() { return 0; }
static inline int sceNpAuthTerm() { return 0; }
static inline int sceNpGetServiceState(SceNpServiceState *state) { if (state) *state = SCE_NP_SERVICE_STATE_OFFLINE; return 0; }
static inline int sceSslInit(uint32_t pool) { return 0; }
static inline int sceSslTerm() { return 0; }

static inline int sceNpTusInit(int32_t a1, uint64_t a2, void *a3) { return 0; }
static inline int sceNpTusTerm() { return 0; }
static inline int sceNpManagerGetNpId(SceNpId *id) { return 0; }
static inline int sceNpTusCreateTitleCtx(void *a1, void *a2, void *a3) { return 1; }
static inline int sceNpTusDeleteTitleCtx(int32_t ctx) { return 0; }
static inline int sceNpTusCreateRequest(int32_t ctx) { return 1; }
static inline int sceNpTusDeleteMultiSlotData(int a1, void *a2, void *a3, int a4, void *a5) { return 0; }
static inline int sceNpTusDeleteRequest(int a1) { return 0; }
static inline int sceNpTusSetData(int a1, void *a2, uint32_t a3, size_t a4, size_t a5, const char *a6, void *a7, size_t a8, void *a9, void *a10, void *a11) { return 0; }
static inline int sceNpTusGetData(int a1, void *a2, uint32_t a3, void *a4, size_t a5, void *a6, size_t a7, void *a8) { return 0; }
static inline int sceNpTusGetMultiSlotDataStatus(int a1, void *a2, const SceNpTusSlotId *a3, void *a4, size_t a5, int a6, void *a7) { return 0; }

#endif
