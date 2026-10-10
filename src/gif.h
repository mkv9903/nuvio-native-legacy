#ifndef NV_GIF_H
#define NV_GIF_H
#include <stddef.h>

// Decode the first frame as a still image. The caller frees the RGBA buffer.
// Output preserves aspect ratio and never enlarges the source image.
unsigned char *gif_primeiro_rgba(const unsigned char *b, size_t n, int largMax,
                                int *w, int *h, int *originalW, int *originalH);

#endif
