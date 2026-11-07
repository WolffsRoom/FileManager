// File Manager entry point. Startup order follows NetStream (GrapheneCt).

#include <sdk_compat.h>
#include <kernel.h>
#include <paf.h>
#include <libsysmodule.h>
#include <psp2/power.h>

#include "common.h"
#include "browser_menu.h"
#include "settings.h"

using namespace paf;

Plugin *g_appPlugin = nullptr;
job::JobQueue *g_jobQueue = nullptr;

namespace {
	// UI callbacks must run on the actual PAF main thread. The old DeferJob
	// ran on FM::JobQueue, merely locking the main-thread mutex, which is NOT
	// the same thing and caused widget-event corruption and crashes.
	struct DeferredUiCall {
		void (*fn)(void *);
		void *userdata;
	};
	static DeferredUiCall s_uiCalls[128];
	static uint32_t s_uiCount = 0;
	static SceUID s_uiMutex = -1;

	void DrainDeferredUi(void *)
	{
		if (s_uiMutex < 0) return;
		DeferredUiCall ready[128];
		uint32_t count = 0;
		sceKernelLockMutex(s_uiMutex, 1, NULL);
		count = s_uiCount;
		for (uint32_t i = 0; i < count; ++i) ready[i] = s_uiCalls[i];
		s_uiCount = 0;
		sceKernelUnlockMutex(s_uiMutex, 1);
		// Dispatch outside the queue lock, so further DeferCalls run next frame.
		for (uint32_t i = 0; i < count; ++i)
			if (ready[i].fn) ready[i].fn(ready[i].userdata);
	}
}

void DeferCall(void (*fn)(void *), void *userdata)
{
	if (!fn || s_uiMutex < 0) return;
	sceKernelLockMutex(s_uiMutex, 1, NULL);
	if (s_uiCount < sizeof(s_uiCalls) / sizeof(s_uiCalls[0]))
	{
		s_uiCalls[s_uiCount].fn = fn;
		s_uiCalls[s_uiCount].userdata = userdata;
		++s_uiCount;
	}
	else
	{
		sceClibPrintf("[FM] deferred UI queue overflow\n");
	}
	sceKernelUnlockMutex(s_uiMutex, 1);
}

static void PluginStart(Plugin *plugin)
{
	if (!plugin)
	{
		sceClibPrintf("[FM] plugin load failed\n");
		return;
	}

	g_appPlugin = plugin;

	settings::Load();
	settings::ApplyLanguage();

	job::JobQueue::Option queueOpt;
	queueOpt.num_thread = 1;
	queueOpt.worker_opt = NULL;
	queueOpt.priority = SCE_KERNEL_COMMON_QUEUE_HIGHEST_PRIORITY;
	queueOpt.stack_size = SCE_KERNEL_256KiB;
	g_jobQueue = new job::JobQueue("FM::JobQueue", &queueOpt);

	s_uiMutex = sceKernelCreateMutex("FM::UiDeferred", 0, 0, NULL);
	if (s_uiMutex >= 0) common::MainThreadCallList::Register(DrainDeferredUi, NULL);

	new BrowserMenu();
}

int main()
{
	// Same CPU clock as FMVita, plus bus and GPU: keeps long lists and transitions smooth.
	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);

	Framework::InitParam fwParam;
	Framework::SampleInit(&fwParam);
	fwParam.mode = Framework::Mode_Normal;
	fwParam.surface_pool_size = SCE_KERNEL_32MiB;
	fwParam.text_surface_pool_size = SCE_KERNEL_4MiB;
	fwParam.graph_heap_size_on_main_memory = 0;
	fwParam.graph_heap_size_on_video_memory = SCE_KERNEL_1MiB;
	fwParam.graphics_option = 7;

	if (SCE_PAF_IS_DOLCE)
	{
		fwParam.env_param = new ui::EnvironmentParam(ui::EnvironmentParam::RESOLUTION_PSP2);
		fwParam.screen_width = 1920;
		fwParam.screen_height = 1088;
	}

	Framework *fw = new Framework(fwParam);
	fw->LoadCommonResourceAsync();

	SceAppUtilInitParam init;
	SceAppUtilBootParam boot;
	sce_paf_memset(&init, 0, sizeof(SceAppUtilInitParam));
	sce_paf_memset(&boot, 0, sizeof(SceAppUtilBootParam));
	sceAppUtilInit(&init, &boot);

	sceSysmoduleLoadModule(SCE_SYSMODULE_FIBER);
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_COMMON_GUI_DIALOG);

	Plugin::InitParam pluginParam;
	pluginParam.name = "file_manager_plugin";
	pluginParam.resource_file = "app0:resource/file_manager_plugin.rco";
	pluginParam.caller_name = "__main__";
	pluginParam.start_func = PluginStart;
	Plugin::LoadAsync(pluginParam);

	fw->Run();

	sceKernelExitProcess(0);
	return 0;
}
