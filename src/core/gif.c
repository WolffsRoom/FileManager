#include <psp2/io/fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "core/gif.h"

#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "legacy/stb_image.h"

#define GIF_MAX_FILE (32 * 1024 * 1024)
#define GIF_MAX_DECODED (48 * 1024 * 1024)

int gifLoad(const char *path, GifAnimation *animation) {
  if (!path || !animation)
    return -1;
  memset(animation, 0, sizeof(*animation));

  SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
  if (fd < 0)
    return fd;
  int64_t size = sceIoLseek(fd, 0, SCE_SEEK_END);
  sceIoLseek(fd, 0, SCE_SEEK_SET);
  if (size <= 0 || size > GIF_MAX_FILE) {
    sceIoClose(fd);
    return -1;
  }

  unsigned char *encoded = malloc((size_t)size);
  if (!encoded) {
    sceIoClose(fd);
    return -1;
  }
  int read = sceIoRead(fd, encoded, (SceSize)size);
  sceIoClose(fd);
  if (read != size) {
    free(encoded);
    return read < 0 ? read : -1;
  }

  int channels = 0;
  int *delays = NULL;
  unsigned char *rgba = stbi_load_gif_from_memory(encoded, read, &delays,
    &animation->width, &animation->height, &animation->frameCount, &channels, 4);
  free(encoded);
  if (!rgba || !delays || animation->width <= 0 || animation->height <= 0 || animation->frameCount <= 0) {
    if (rgba) stbi_image_free(rgba);
    if (delays) free(delays);
    memset(animation, 0, sizeof(*animation));
    return -1;
  }

  uint64_t decoded = (uint64_t)animation->width * animation->height * animation->frameCount * 4;
  if (decoded > GIF_MAX_DECODED) {
    stbi_image_free(rgba);
    free(delays);
    memset(animation, 0, sizeof(*animation));
    return -1;
  }
  for (int i = 0; i < animation->frameCount; ++i) {
    // GIFs commonly encode 0/10 ms delays. Browsers clamp these so playback
    // stays visible and does not monopolize the Vita's UI thread.
    if (delays[i] < 20)
      delays[i] = 100;
  }
  animation->rgba = rgba;
  animation->delaysMs = delays;
  return 0;
}

void gifFree(GifAnimation *animation) {
  if (!animation)
    return;
  if (animation->rgba)
    stbi_image_free(animation->rgba);
  free(animation->delaysMs);
  memset(animation, 0, sizeof(*animation));
}
