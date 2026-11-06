#ifndef _INI_FILE_PROCESSOR_H_
#define _INI_FILE_PROCESSOR_H_

#include <stdint.h>
#include <stddef.h>

#define SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE 256

namespace sce {
	namespace Ini {
		struct MemAllocator {
			void *(*allocate)(size_t);
			void (*deallocate)(void *);
		};

		struct InitParameter {
			uint32_t workmemSize;
			uint32_t unk_0x4;
			MemAllocator *allocator;
		};

		class IniFileProcessor {
		public:
			IniFileProcessor() {}
			~IniFileProcessor() {}

			int32_t initialize(InitParameter *param) { return 0; }
			int32_t open(const char *path, const char *mode, int flag = 0) { return 0; }
			uint32_t size() { return 0; }
			int32_t parse(char *key, char *val, uint32_t sz) { return 0; }
			int32_t del(const char *key) { return 0; }
			int32_t reset() { return 0; }
			int32_t add(const char *key, const char *val) { return 0; }
			int32_t get(const char *key, char *val, uint32_t sz, int unk = 0) { return 0; }
			int32_t close() { return 0; }
			int32_t flush() { return 0; }
			int32_t terminate() { return 0; }
		};
	}
}

using namespace sce;

#endif
