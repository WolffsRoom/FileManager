#ifndef _DOWNLOAD_SERVICE_H_
#define _DOWNLOAD_SERVICE_H_

#include <stdint.h>
#include <stddef.h>
#include <ipmi.h>

namespace sce {
	namespace Download {
		enum ContentType {
			ContentType_Multimedia = 0
		};

		enum Method {
			Method_GetHeaderInfo = 0,
			Method_Download = 1
		};

		struct ConnectionOpt {
			uint32_t budgetType;
		};

		struct GetHeaderInfoParam {
			uint32_t contentType;
			uint32_t resolveTimeout;
			uint32_t resolveRetry;
			uint32_t connectTimeout;
			uint32_t sendTimeout;
			uint32_t recvTimeout;
			uint8_t url[2048];
		};

		struct AuthParam {};

		struct DownloadParam {
			uint32_t bgdlLocation;
			uint8_t url[2048];
		};

		struct HeaderInfo {
			uint8_t name[256];
		};

		struct Download {
			void *clientMem;
			void *bufMem;
			IPMI::Client *client;
			int32_t unk_00;
			int32_t unk_04;
			int32_t unk_08;
		};
	}
}

#endif
