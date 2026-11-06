/* PSARC browsing through the Vita's native SceFios2 dearchiver.
   Adapted from VitaShell/FMVita (TheFloW), GPLv3. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "psarc.h"

#define FM_PATH_MAX 1024
#define FIOS_FH_SIZE 80
#define FIOS_DH_SIZE 80
#define FIOS_OP_SIZE 168
#define FIOS_CHUNK_SIZE 64
#define ALIGN_UP(v, a) (((v) + ((a) - 1)) & ~((a) - 1))
#define STORAGE_SIZE(n, s) (((n) * (s)) + ALIGN_UP(ALIGN_UP((n), 8) / 8, 8))

typedef int32_t SceFiosFH;
typedef int32_t SceFiosDH;
typedef int64_t SceFiosSize;
typedef struct { void *pPtr; size_t length; } SceFiosBuffer;
typedef struct {
	size_t sizeOfContext, workBufferSize;
	void *pWorkBuffer;
	intptr_t flags, reserved[3];
} SceFiosPsarcDearchiverContext;
typedef struct {
	uint32_t initialized : 1, paramsSize : 15, pathMax : 16;
	uint32_t profiling, ioThreadCount, threadsPerScheduler;
	uint32_t extraFlag1 : 1, extraFlags : 31;
	uint32_t maxChunk;
	uint8_t maxDecompressorThreadCount, reserved1, reserved2, reserved3;
	intptr_t reserved4, reserved5;
	SceFiosBuffer opStorage, fhStorage, dhStorage, chunkStorage;
	void *pVprintf, *pMemcpy, *pProfileCallback;
	int threadPriority[3], threadAffinity[3], threadStackSize[3];
} SceFiosParams;
typedef struct {
	SceFiosSize fileSize;
	uint32_t statFlags;
	uint16_t nameLength, fullPathLength, offsetToName, reserved[3];
	char fullPath[FM_PATH_MAX];
} SceFiosDirEntry;

extern int sceFiosInitialize(const SceFiosParams *params);
extern void sceFiosTerminate(void);
extern SceFiosSize sceFiosArchiveGetMountBufferSizeSync(const void *, const char *, void *);
extern int sceFiosArchiveMountSync(const void *, SceFiosFH *, const char *, const char *, SceFiosBuffer, void *);
extern int sceFiosArchiveUnmountSync(const void *, SceFiosFH);
extern int sceFiosDHOpenSync(const void *, SceFiosDH *, const char *, SceFiosBuffer);
extern int sceFiosDHReadSync(const void *, SceFiosDH, SceFiosDirEntry *);
extern int sceFiosDHCloseSync(const void *, SceFiosDH);
extern int sceFiosIOFilterAdd(int, void (*)(), void *);
extern int sceFiosIOFilterRemove(int);
extern void sceFiosIOFilterPsarcDearchiver(void);

static int64_t s_ops[STORAGE_SIZE(64, FIOS_OP_SIZE + FM_PATH_MAX) / 8 + 1];
static int64_t s_chunks[STORAGE_SIZE(1024, FIOS_CHUNK_SIZE) / 8 + 1];
static int64_t s_fhs[STORAGE_SIZE(32, FIOS_FH_SIZE + FM_PATH_MAX) / 8 + 1];
static int64_t s_dhs[STORAGE_SIZE(32, FIOS_DH_SIZE + FM_PATH_MAX) / 8 + 1];
static char s_work[0x30000] __attribute__((aligned(64)));
static SceFiosPsarcDearchiverContext s_context;
static SceFiosBuffer s_mountBuffer;
static SceFiosFH s_archive = -1;
static char s_path[FM_PATH_MAX];

int psarcOpen(const char *path)
{
	if (s_archive >= 0) psarcClose();
	SceFiosParams p = { 0 };
	p.paramsSize = sizeof(p);
	p.pathMax = FM_PATH_MAX;
	p.ioThreadCount = 2;
	p.threadsPerScheduler = 1;
	p.maxChunk = 256 * 1024;
	p.maxDecompressorThreadCount = 2;
	p.opStorage = (SceFiosBuffer){ s_ops, sizeof(s_ops) };
	p.fhStorage = (SceFiosBuffer){ s_fhs, sizeof(s_fhs) };
	p.dhStorage = (SceFiosBuffer){ s_dhs, sizeof(s_dhs) };
	p.chunkStorage = (SceFiosBuffer){ s_chunks, sizeof(s_chunks) };
	p.threadPriority[0] = 66; p.threadPriority[1] = 189; p.threadPriority[2] = 66;
	p.threadAffinity[0] = 0x40000; p.threadAffinity[2] = 0x40000;
	p.threadStackSize[0] = 8192; p.threadStackSize[1] = 16384; p.threadStackSize[2] = 8192;
	int res = sceFiosInitialize(&p);
	if (res < 0) return res;
	s_context = (SceFiosPsarcDearchiverContext){ sizeof(s_context), sizeof(s_work), s_work, 0, { 0, 0, 0 } };
	res = sceFiosIOFilterAdd(0, sceFiosIOFilterPsarcDearchiver, &s_context);
	if (res < 0) { sceFiosTerminate(); return res; }
	SceFiosSize need = sceFiosArchiveGetMountBufferSizeSync(NULL, path, NULL);
	if (need < 0) { sceFiosIOFilterRemove(0); sceFiosTerminate(); return (int)need; }
	s_mountBuffer.length = (size_t)need;
	s_mountBuffer.pPtr = malloc(s_mountBuffer.length);
	if (!s_mountBuffer.pPtr) { sceFiosIOFilterRemove(0); sceFiosTerminate(); return -1; }
	res = sceFiosArchiveMountSync(NULL, &s_archive, path, path, s_mountBuffer, NULL);
	if (res < 0) {
		free(s_mountBuffer.pPtr); s_mountBuffer.pPtr = NULL;
		sceFiosIOFilterRemove(0); sceFiosTerminate(); return res;
	}
	strncpy(s_path, path, sizeof(s_path) - 1);
	s_path[sizeof(s_path) - 1] = 0;
	return 0;
}

int psarcClose(void)
{
	if (s_archive < 0) return 0;
	sceFiosArchiveUnmountSync(NULL, s_archive);
	s_archive = -1;
	free(s_mountBuffer.pPtr); s_mountBuffer.pPtr = NULL; s_mountBuffer.length = 0;
	sceFiosIOFilterRemove(0);
	sceFiosTerminate();
	s_path[0] = 0;
	return 0;
}

int psarcIsOpen(void) { return s_archive >= 0; }
int psarcContainsPath(const char *path)
{
	size_t n = strlen(s_path);
	return n && !strncmp(path, s_path, n) && (path[n] == 0 || path[n] == '/');
}

int psarcList(const char *path, PsarcListCallback callback, void *userdata)
{
	SceFiosDH dh = -1;
	SceFiosBuffer empty = { 0, 0 };
	int res = sceFiosDHOpenSync(NULL, &dh, path, empty);
	if (res < 0) return res;
	for (;;) {
		SceFiosDirEntry entry;
		memset(&entry, 0, sizeof(entry));
		res = sceFiosDHReadSync(NULL, dh, &entry);
		if (res < 0) break;
		const char *name = entry.fullPath + entry.offsetToName;
		if (name[0] && strcmp(name, ".") && strcmp(name, ".."))
			callback(name, (entry.statFlags & 1) != 0, (uint64_t)entry.fileSize, userdata);
	}
	sceFiosDHCloseSync(NULL, dh);
	return 0;
}
