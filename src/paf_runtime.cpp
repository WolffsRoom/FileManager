// ScePaf bootstrap for a VitaSDK (GCC/newlib) build. Based on paf_runtime.cpp from
// NetStream (GrapheneCt) and BetterHomebrewBrowser (Ibrahim778).
//
// - preloadPaf() loads ScePaf before any other static constructor, because the
//   global PAF heap must exist before the first sce_paf_* or Framework call.
// - operator new/delete go to the PAF heap. ScePaf frees objects that the
//   application allocated (and the reverse), so both sides must share one heap.
//   newlib malloc stays available for the plain C code in src/core.

#include <new>
#include <kernel.h>
#include <paf.h>
#include <libsysmodule.h>

extern "C" {
	unsigned int sceUserMainThreadStackSize = 1024 * 1024;
	// Heap for newlib malloc (C core: copy buffers, directory walks).
	int _newlib_heap_size_user = 32 * 1024 * 1024;
}

void *operator new(std::size_t size) { return sce_paf_malloc(size); }
void *operator new[](std::size_t size) { return sce_paf_malloc(size); }
void *operator new(std::size_t size, const std::nothrow_t &) noexcept { return sce_paf_malloc(size); }
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept { return sce_paf_malloc(size); }
void operator delete(void *ptr) noexcept { sce_paf_free(ptr); }
void operator delete[](void *ptr) noexcept { sce_paf_free(ptr); }
void operator delete(void *ptr, std::size_t) noexcept { sce_paf_free(ptr); }
void operator delete[](void *ptr, std::size_t) noexcept { sce_paf_free(ptr); }

__attribute__((constructor(101))) void preloadPaf()
{
	int32_t load_res = 0;
	ScePafInit init_param;
	SceSysmoduleOpt opt;

	// All fields are read by ScePaf. Do not use sce_paf_memset: not loaded yet.
	init_param.global_heap_size = 32 * 1024 * 1024;
	init_param.reserved[0] = 0x0000EA60;
	init_param.reserved[1] = 0x00040000;
	init_param.cdlg_mode = 0;
	init_param.global_heap_alignment = 0;
	init_param.global_heap_disable_assert_on_alloc_failure = 0;

	opt.flags = 0;
	opt.result = &load_res;

	int32_t ret = sceSysmoduleLoadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF, sizeof(init_param), &init_param, &opt);
	if (ret < 0) {
		sceClibPrintf("[FM] ScePaf load failed: 0x%x result 0x%x\n", ret, load_res);
	}
}
