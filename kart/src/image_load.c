/*
 * image_load.c - see image_load.h. Built for both PLATFORM_PC and
 * PLATFORM_GC: stb_image only needs fopen/fread/fseek, which the "gcdvd:"
 * device (gcdvd.c) provides on GameCube just like the OS filesystem does
 * on desktop.
 */
#if defined(PLATFORM_PC) || defined(PLATFORM_GC)

#if defined(__GNUC__)
  /* Third-party header; don't let its warnings show up under our -Wall
     -Wextra (this project's own code stays warning-clean, but we don't
     police vendored code we don't maintain). */
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-function"
  #pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "third_party/stb_image.h"

#if defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include "image_load.h"

unsigned char *image_load_rgba(const char *path, int *out_w, int *out_h) {
    int w = 0, h = 0, channels_in_file = 0;
    unsigned char *pixels = stbi_load(path, &w, &h, &channels_in_file, 4);
    if (!pixels) return NULL;
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    return pixels;
}

void image_free(unsigned char *pixels) {
    stbi_image_free(pixels);
}

#endif /* PLATFORM_PC || PLATFORM_GC */
