#ifndef _PAF_FILE_EXT_H_
#define _PAF_FILE_EXT_H_

#include <paf/file/file.h>
#include <paf/file/localfile.h>

namespace paf {
	class CurlFile : public File {
	public:
		class OpenArg : public File::OpenArg {
		public:
			enum OptionType {
				OptionType_ConnectTimeOut = 0,
				OptionType_RecvTimeOut = 1
			};

			void ParseUrl(const char *url) {}
			void SetOption(int val, OptionType type) {}
			void SetProxy(const char *proxy) {}
			void SetShare(void *share) {}
		};

		class Share {
		public:
			static Share *Create() { return new Share(); }
			void AddRef() {}
		};

		CurlFile() {}
		virtual ~CurlFile() {}

		virtual int32_t GetFileType() const { return 0; }
		virtual int32_t GetCapability() const { return 0; }
		virtual int64_t GetFileSize() const { return 0; }
		virtual bool IsOpened() const { return false; }
		virtual bool IsEof() { return false; }
		virtual int32_t Abort() { return 0; }

		virtual int32_t Open(const File::OpenArg *param) { return 0; }
		virtual int32_t OpenAsync(const File::OpenArg *param) { return 0; }
		virtual int32_t Close() { return 0; }
		virtual int32_t CloseAsync() { return 0; }
		virtual int32_t Read(void *buf, size_t nbyte) { return 0; }
		virtual int32_t ReadAsync(void *buf, size_t nbyte) { return 0; }
		virtual int32_t Write(const void *buf, size_t nbyte) { return 0; }
		virtual int32_t WriteAsync(const void *buf, size_t nbyte) { return 0; }
		virtual int64_t Seek(int64_t offset, int32_t whence) { return 0; }
		virtual int32_t SeekAsync(int64_t offset, int32_t whence) { return 0; }
		virtual int32_t Flush() { return 0; }
		virtual int32_t FlushAsync() { return 0; }
		virtual int32_t WaitAsync(int64_t *result) { return 0; }
		virtual int32_t PollAsync(int64_t *result) { return 0; }
		virtual int32_t Getstat(FileStat *stat) { return 0; }
		virtual int32_t SetPriority(int32_t priority) { return 0; }
	};
}

#endif
