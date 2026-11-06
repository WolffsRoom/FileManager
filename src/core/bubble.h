#ifndef _FM_BUBBLE_H_
#define _FM_BUBBLE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Turns a square PNG (an app's icon0.png) into the home screen's round bubble:
 * circular alpha with a soft edge and a light gloss on top. Returns a newly
 * malloc'ed PNG in *out (size in *out_size), or a negative value on failure.
 */
int makeBubblePng(const void *png, uint32_t size, void **out, uint32_t *out_size);

#ifdef __cplusplus
}
#endif

#endif
