#ifndef _NETCHECK_DIALOG_INTERNAL_H_
#define _NETCHECK_DIALOG_INTERNAL_H_

#include <psp2/netcheck_dialog.h>

#ifndef SCE_NETCHECK_DIALOG_ERROR_PARAM
#define SCE_NETCHECK_DIALOG_ERROR_PARAM 0
#endif

#ifndef SCE_NETCHECK_DIALOG_LEAST_SSL_POOL_SIZE
#define SCE_NETCHECK_DIALOG_LEAST_SSL_POOL_SIZE (64 * 1024)
#endif

typedef int (*SceNetcheckDialogCallback)(void *data);

typedef struct SceNetCheckDialogParamInternal {
	uint32_t mode;
	uint32_t unk_00;
	uint32_t skipSignIn;
} SceNetCheckDialogParamInternal;

static inline int sceNetCheckDialogParamInitInternal(SceNetCheckDialogParam *param, SceNetCheckDialogParamInternal *iparam) { return 0; }
static inline int sceNetCheckDialogInitInternal(SceNetCheckDialogParam *param, SceNetCheckDialogParamInternal *iparam) { return 0; }
static inline int sceNetcheckDialogInit(void *param) { return 0; }
static inline int sceNetcheckDialogGetStatus() { return 0; }
static inline int sceNetcheckDialogTerm() { return 0; }
static inline int sceNetcheckDialogAbort() { return 0; }

#endif
