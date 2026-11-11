#include <psp2/io/stat.h>
#include <psp2/musicexport.h>
#include <psp2/videoexport.h>
#include <psp2/sysmodule.h>
#include <stdlib.h>
#include <string.h>

#include "legacy/vitashell_error.h"
#include "core/media_export.h"

typedef struct ExportProgress {
  FileProcessParam *param;
  uint64_t size;
  uint64_t reported;
} ExportProgress;

static void onExportProgress(void *userdata, int percent) {
  ExportProgress *state = (ExportProgress *)userdata;
  if (!state || !state->param) return;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  uint64_t now = state->size * (uint64_t)percent / 100;
  if (now > state->reported && state->param->value)
    *state->param->value += now - state->reported;
  if (now > state->reported) state->reported = now;
  if (state->param->SetProgress)
    state->param->SetProgress(state->param->value ? *state->param->value : now, state->param->max);
}

int exportMediaFile(const char *path, int video, FileProcessParam *param) {
  SceIoStat stat;
  memset(&stat, 0, sizeof(stat));
  int res = sceIoGetstat(path, &stat);
  if (res < 0) return res;

  int module = video ? SCE_SYSMODULE_VIDEO_EXPORT : SCE_SYSMODULE_MUSIC_EXPORT;
  res = sceSysmoduleLoadModule(module);
  // Already loaded is not a failure for this operation.
  if (res < 0 && (uint32_t)res != 0x805A1002) return res;

  void *work = malloc(64 * 1024);
  if (!work) return VITASHELL_ERROR_NO_MEMORY;
  ExportProgress progress = { param, stat.st_size, 0 };

  if (video) {
    VideoExportInputParam input;
    VideoExportOutputParam output;
    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    strncpy(input.path, path, sizeof(input.path) - 1);
    res = sceVideoExportFromFile(&input, 1, work,
      param ? param->cancelHandler : NULL, onExportProgress, &progress, 0, &output);
    if ((uint32_t)res == 0x8010540A) res = 0; // already exported
  } else {
    MusicExportParam input;
    char output[1024];
    memset(&input, 0, sizeof(input));
    memset(output, 0, sizeof(output));
    res = sceMusicExportFromFile(path, &input, work,
      param ? param->cancelHandler : NULL, onExportProgress, &progress, output, sizeof(output));
    if ((uint32_t)res == 0x8010530A) res = 0; // already exported
  }

  if (res >= 0 && progress.reported < progress.size) {
    uint64_t rest = progress.size - progress.reported;
    if (param && param->value) *param->value += rest;
    if (param && param->SetProgress)
      param->SetProgress(param->value ? *param->value : progress.size, param->max);
  }
  free(work);
  return res < 0 ? res : 1;
}
