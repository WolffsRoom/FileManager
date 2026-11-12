#include <string.h>
#include <strings.h>
#include <psp2/appmgr.h>

#include "legacy/audio/player.h"
#include "legacy/audio/vita_audio.h"
#include "core/music.h"

static int s_audioReady = 0;
static int s_open = 0;
static int s_paused = 0;
static double s_duration = 0.0;
static double s_fileSize = 0.0;

static int typeFor(const char *path) {
  const char *dot = strrchr(path, '.');
  if (!dot)
    return UNK_TYPE;
  if (strcasecmp(dot, ".mp3") == 0)
    return MP3_TYPE;
  if (strcasecmp(dot, ".ogg") == 0)
    return OGG_TYPE;
  return UNK_TYPE;
}

int musicSupported(const char *path) {
  return typeFor(path) != UNK_TYPE;
}

int musicOpen(const char *path, MusicInfo *info) {
  int type = typeFor(path);
  if (type == UNK_TYPE)
    return -1;

  if (!s_audioReady) {
    vitaAudioInit(0x40);
    s_audioReady = 1;
  }
  musicClose();

  sceAppMgrAcquireBgmPort();
  if (setAudioFunctions(type) < 0)
    return -1;
  initFunct(0);
  if (loadFunct((char *)path) != OPENING_OK) {
    endFunct();
    return -1;
  }
  playFunct();
  s_open = 1;
  s_paused = 0;

  memset(info, 0, sizeof(*info));
  struct fileInfo *fi = getInfoFunct();
  if (fi) {
    strncpy(info->title, fi->title, sizeof(info->title) - 1);
    strncpy(info->artist, fi->artist, sizeof(info->artist) - 1);
    strncpy(info->album, fi->album, sizeof(info->album) - 1);
    strncpy(info->length, fi->strLength, sizeof(info->length) - 1);
    info->seconds = fi->length;
    s_duration = fi->length;
    s_fileSize = fi->fileSize;
    info->coverType = fi->encapsulatedPictureType;
    info->coverOffset = fi->encapsulatedPictureOffset;
    info->coverLength = fi->encapsulatedPictureLength;
  }
  return 0;
}

void musicClose(void) {
  if (!s_open)
    return;
  endFunct();
  unsetAudioFunctions();
  sceAppMgrReleaseBgmPort();
  setVizPcmCapture(0);
  s_open = 0;
  s_duration = 0.0;
  s_fileSize = 0.0;
}

void musicSetWaveformEnabled(int enabled) {
  setVizPcmCapture(enabled);
}

int musicWaveform(float *bars, int count) {
  if (!bars || count <= 0) return 0;
  short *pcm = NULL;
  int samples = 0;
  getVizPcmData(&pcm, &samples);
  if (!pcm || samples <= 0) return 0;
  for (int i = 0; i < count; ++i) {
    int begin = i * samples / count;
    int end = (i + 1) * samples / count;
    if (end <= begin) end = begin + 1;
    int peak = 0;
    for (int j = begin; j < end && j < samples; ++j) {
      int left = pcm[j * 2];
      int right = pcm[j * 2 + 1];
      if (left < 0) left = -left;
      if (right < 0) right = -right;
      if (left > peak) peak = left;
      if (right > peak) peak = right;
    }
    bars[i] = (float)peak / 32768.0f;
  }
  return 1;
}

void musicTogglePause(void) {
  if (!s_open)
    return;
  /* The engine's Pause toggles; Play resumes after a pause. */
  if (isPlayingFunct()) {
    pauseFunct();
    s_paused = 1;
  } else {
    playFunct();
    s_paused = 0;
  }
}

int musicIsPlaying(void) {
  return s_open && isPlayingFunct();
}

int musicEnded(void) {
  return s_open && (endOfStreamFunct() || getPercentageFunct() >= 100.0f);
}

float musicPercent(void) {
  return s_open ? getPercentageFunct() : 0.0f;
}

void musicTime(char *out) {
  out[0] = 0;
  if (s_open)
    getTimeStringFunct(out);
}

void musicSeek(double seconds) {
  if (!s_open || !setFilePositionFunct || s_duration <= 0.0 || s_fileSize <= 0.0)
    return;
  if (seconds < 0)
    seconds = 0;
  if (seconds > s_duration)
    seconds = s_duration;
  setFilePositionFunct(s_fileSize * seconds / s_duration);
}
