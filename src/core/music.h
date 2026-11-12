#ifndef _FM_MUSIC_H_
#define _FM_MUSIC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge to FMVita's audio engine (legacy/audio, libmad and libvorbis). */

typedef struct {
  char title[260];
  char artist[260];
  char album[260];
  char length[12];     /* "mm:ss" */
  long seconds;
  int coverType;       /* 0 none, 1 JPEG, 2 PNG (embedded picture) */
  int coverOffset;
  int coverLength;
} MusicInfo;

/* MP3 or OGG by extension. Returns 0 on success. */
int musicOpen(const char *path, MusicInfo *info);
void musicClose(void);
void musicTogglePause(void);
int musicIsPlaying(void);
int musicEnded(void);
float musicPercent(void);
void musicTime(char *out);            /* elapsed "mm:ss" */
void musicSeek(double seconds);
void musicSetWaveformEnabled(int enabled);
int musicWaveform(float *bars, int count);

/* Whether the engine supports this file. */
int musicSupported(const char *path);

#ifdef __cplusplus
}
#endif

#endif
