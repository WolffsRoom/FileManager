#ifndef _FILES_SDK_COMPAT_H_
#define _FILES_SDK_COMPAT_H_

#include <stdarg.h>
typedef va_list _Va_list;
class PredictiveFile;

#ifndef SCE_CDECL_BEGIN
#ifdef __cplusplus
#define SCE_CDECL_BEGIN extern "C" {
#define SCE_CDECL_END }
#else
#define SCE_CDECL_BEGIN
#define SCE_CDECL_END
#endif
#endif

#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/sysmodule.h>
#include <psp2/power.h>
#include <psp2/types.h>

#ifndef SCE_POWER_CALLBACKARG_RESERVED_22
#define SCE_POWER_CALLBACKARG_RESERVED_22 0x00400000
#endif

#ifndef SCE_POWER_CALLBACKARG_RESERVED_23
#define SCE_POWER_CALLBACKARG_RESERVED_23 0x00800000
#endif

#ifndef SCE_DBG_LOG_COMPONENT
#define SCE_DBG_LOG_COMPONENT "[Files]"
#endif

#ifndef SCE_DBG_LOG_INFO
#define SCE_DBG_LOG_INFO(...)   do { sceClibPrintf(SCE_DBG_LOG_COMPONENT " "); sceClibPrintf(__VA_ARGS__); } while (0)
#endif

#ifndef SCE_DBG_LOG_ERROR
#define SCE_DBG_LOG_ERROR(...)  do { sceClibPrintf(SCE_DBG_LOG_COMPONENT " ERRO "); sceClibPrintf(__VA_ARGS__); } while (0)
#endif

#ifndef SCE_DBG_LOG_WARNING
#define SCE_DBG_LOG_WARNING(...) do { sceClibPrintf(SCE_DBG_LOG_COMPONENT " AVISO "); sceClibPrintf(__VA_ARGS__); } while (0)
#endif


#ifndef SCE_ERROR_ERRNO_ENOENT
#define SCE_ERROR_ERRNO_ENOENT 0x80010002
#endif

#ifndef SCE_ERROR_ERRNO_EINVAL
#define SCE_ERROR_ERRNO_EINVAL 0x80010016
#endif

#ifndef SCE_ERROR_ERRNO_ENOMEM
#define SCE_ERROR_ERRNO_ENOMEM 0x8001000c
#endif

#ifndef SCE_ERROR_ERRNO_ENOTDIR
#define SCE_ERROR_ERRNO_ENOTDIR 0x80010014
#endif

#ifndef SCE_ERROR_ERRNO_ECONNREFUSED
#define SCE_ERROR_ERRNO_ECONNREFUSED 0x8001006f
#endif

#ifndef SCE_ERROR_ERRNO_EUNSUP
#define SCE_ERROR_ERRNO_EUNSUP 0x80010086
#endif

#ifndef SCE_S_ISDIR
#define SCE_S_ISDIR(m) (((m) & SCE_S_IFMT) == SCE_S_IFDIR)
#endif

#ifndef SCE_KERNEL_1MiB
#define SCE_KERNEL_1MiB  (1 * 1024 * 1024)
#define SCE_KERNEL_4MiB  (4 * 1024 * 1024)
#define SCE_KERNEL_16MiB (16 * 1024 * 1024)
#define SCE_KERNEL_32MiB (32 * 1024 * 1024)
#endif

#endif
