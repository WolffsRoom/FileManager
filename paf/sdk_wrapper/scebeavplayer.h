#ifndef _SCEBEAVPLAYER_H_
#define _SCEBEAVPLAYER_H_
#include <stdint.h>
#include <stddef.h>
#include <curl/curl.h>

typedef int32_t LSResult;
typedef int32_t LSInputResult;
typedef void LSSession;
typedef void LSStreamlist;
typedef void LSStreamfile;
typedef void LSLibraryInitParams;
typedef int32_t LSStatus;
typedef void* SceBeavCorePlayerHandle;
typedef void SceBeavCorePlayerLsInputPluginInterface;

typedef struct SceBeavCorePlayerLibLSInterface {
	void *unk;
} SceBeavCorePlayerLibLSInterface;

typedef int32_t tai_hook_ref_t;

static inline int32_t sceBeavCorePlayerInitialize() { return 0; }
#endif
