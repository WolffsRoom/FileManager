#ifndef _LIBDBG_H_
#define _LIBDBG_H_
#include <psp2/kernel/clib.h>
#ifndef SCE_DBG_LOG_COMPONENT
#define SCE_DBG_LOG_COMPONENT "[NetStream]"
#endif
#define SCE_DBG_LOG_INFO(...) do { sceClibPrintf(__VA_ARGS__); } while (0)
#define SCE_DBG_LOG_ERROR(...) do { sceClibPrintf(__VA_ARGS__); } while (0)
#define SCE_DBG_LOG_WARNING(...) do { sceClibPrintf(__VA_ARGS__); } while (0)
#define SCE_DBG_LOG_LEVEL_TRACE 0
#define sceDbgSetMinimumLogLevel(lvl)
#endif
