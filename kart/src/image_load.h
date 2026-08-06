/*
 * image_load.h - runtime PNG loading (desktop only).
 *
 * Thin wrapper around the vendored stb_image.h (third_party/stb_image.h,
 * public domain) so the rest of the codebase never touches that header
 * directly - same pattern as obj_loader.h wrapping OBJ parsing.
 *
 * Only available on platforms with a filesystem (PLATFORM_PC); GameCube
 * textures are decoded once, offline, by tools/obj2c.c and baked into a
 * header as raw RGBA bytes, so it never needs a PNG decoder on-device.
 */
#ifndef IMAGE_LOAD_H
#define IMAGE_LOAD_H

/*
 * Loads `path` and always returns a top-to-bottom row order,
 * 4-channels-per-pixel (RGBA8) buffer, regardless of the source PNG's
 * channel count. On success returns a heap buffer (free with image_free)
 * and sets *out_w and *out_h; returns NULL on failure.
 */
unsigned char *image_load_rgba(const char *path, int *out_w, int *out_h);
void           image_free(unsigned char *pixels);

#endif /* IMAGE_LOAD_H */
