#ifndef _KERNEL_H_
#define _KERNEL_H_
#include <psp2/types.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/clib.h>

#ifndef SCE_UID_INVALID_UID
#define SCE_UID_INVALID_UID (-1)
#endif

#ifndef SCE_KERNEL_DEFAULT_PRIORITY_USER
#define SCE_KERNEL_DEFAULT_PRIORITY_USER 0x10000100
#endif

#ifndef SCE_KERNEL_HIGHEST_PRIORITY_USER
#define SCE_KERNEL_HIGHEST_PRIORITY_USER 64
#endif

#ifndef SCE_KERNEL_ATTR_MULTI
#define SCE_KERNEL_ATTR_MULTI 0x00001000U
#endif

#ifndef SCE_KERNEL_EVF_WAITMODE_OR
#define SCE_KERNEL_EVF_WAITMODE_OR 0x00000001U
#endif

#endif
