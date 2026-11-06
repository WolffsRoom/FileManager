#ifndef _FM_COMMON_H_
#define _FM_COMMON_H_

#include <sdk_compat.h>
#include <paf.h>

#include "file_manager_plugin.h"

extern paf::Plugin *g_appPlugin;
extern paf::job::JobQueue *g_jobQueue;

/**
 * Runs fn(obj) on the next frame instead of now. A page must never be closed from
 * inside one of its own widget callbacks: the widget that is running the callback
 * would be destroyed under it.
 */
void DeferCall(void (*fn)(void *), void *obj);

template <typename T>
inline void DeferDelete(T *obj)
{
	DeferCall([](void *p) { delete static_cast<T *>(p); }, obj);
}

#endif
