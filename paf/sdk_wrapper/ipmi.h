#ifndef _IPMI_H_
#define _IPMI_H_

#include <stdint.h>
#include <stddef.h>

namespace IPMI {
	enum BudgetType {
		BudgetType_Default = 0
	};

	struct DataInfo {
		void *data;
		uint32_t dataSize;
	};

	struct BufferInfo {
		void *data;
		uint32_t dataSize;
	};

	class Client {
	public:
		struct Config {
			uint8_t serviceName[32];
			uint32_t pipeBudgetType;
			uint32_t numResponses;
			uint32_t requestQueueSize;
			uint32_t receptionQueueSize;
			uint32_t numAsyncResponses;
			uint32_t requestQueueAddsize1;
			uint32_t requestQueueAddsize2;
			uint32_t numEventFlags;
			uint32_t msgQueueSize;
			int32_t serverPID;

			uint32_t estimateClientMemorySize() const { return 0x1000; }
		};

		static int32_t create(Client **client, Config const *conf, void *user, void *mem) {
			*client = new Client();
			return 0;
		}

		int32_t connect(void *opt, uint32_t sz, int32_t *ret) { return 0; }
		int32_t disconnect() { return 0; }
		int32_t destroy() { return 0; }
		int32_t invokeSyncMethod(uint32_t method, DataInfo *dtInfo, uint32_t dtCount, int32_t *ret2, BufferInfo *bfInfo, uint32_t bfCount) { return 0; }
	};
}

#endif
