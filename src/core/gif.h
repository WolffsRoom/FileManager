#ifndef _FM_GIF_H_
#define _FM_GIF_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GifAnimation {
  int width;
  int height;
  int frameCount;
  unsigned char *rgba;
  int *delaysMs;
} GifAnimation;

/** Decodes all frames of a GIF into contiguous RGBA8888 memory. */
int gifLoad(const char *path, GifAnimation *animation);
void gifFree(GifAnimation *animation);

#ifdef __cplusplus
}
#endif

#endif
