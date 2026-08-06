/*
 * render.h - platform-agnostic rendering interface.
 *
 * Two implementations satisfy this same interface, selected at compile time:
 *   render_gl.c  -> desktop (SDL2 + OpenGL fixed-function immediate mode)
 *   render_gx.c  -> GameCube (libogc GX)
 *
 * The interface is intentionally tiny and uses only fixed-function style
 * primitives (camera + simple shapes) so it maps cleanly onto GX later.
 */
#ifndef RENDER_H
#define RENDER_H

#include "math3d.h"
#include "obj_loader.h"

/* Called once after the GL context exists. */
void render_init(int fb_width, int fb_height);
void render_shutdown(void);

/* Frame boundaries: clear / present is handled by the platform layer. */
void render_begin_frame(void);
void render_end_frame(void);

/* Update the viewport (e.g. on window resize). */
void render_set_viewport(int fb_width, int fb_height);

/* Set the current camera. Loads projection + view onto the GL matrix stack. */
void render_set_camera(Vec3 eye, Vec3 target, Vec3 up, float fov_deg);

/* Draw an axis-aligned box centered at `pos`, rotated by `yaw` about Y. */
void render_draw_box(Vec3 pos, Vec3 half_extents, float yaw,
                     float r, float g, float b);

/* Draw a checkerboard ground plane on the XZ plane at height `y`. */
void render_draw_ground(float y, float half_size, float tile);

/*
 * A single 2D texture, uploaded once and reused across frames. Backend
 * fields are plain data (no opaque pointers) so the struct can be a value
 * member of ModelInstance without either renderer needing to see the
 * other's internals: render_gl.c only touches gl_id, render_gx.c only
 * touches gx_handle (heap-allocated there; NULL here means "no texture").
 */
typedef struct {
    int          loaded;
    int          width, height;
    unsigned int gl_id;
    void        *gx_handle;
} Texture;

/*
 * Uploads a top-to-bottom-row, 4-bytes-per-pixel (RGBA8) image as a GPU
 * texture. Returns 1 on success. `rgba` is only read during the call, not
 * retained. Width/height need not be a power of two.
 */
int  render_texture_create(Texture *tex, const unsigned char *rgba, int width, int height);
void render_texture_destroy(Texture *tex);

/*
 * Draw a loaded mesh at `pos`, rotated by `yaw` about Y and uniformly
 * scaled by `scale`. If `tex` is non-NULL and loaded, its texels (sampled
 * with each vertex's UV) are modulated by the same per-vertex directional
 * shading used everywhere else; otherwise falls back to the mesh's flat
 * per-material color, exactly like before textures existed.
 */
void render_draw_mesh(const Mesh *m, Vec3 pos, float yaw, float scale, const Texture *tex);

/*
 * 2D HUD text overlay, drawn with the built-in 5x7 bitmap font (font.h).
 * Call render_begin_ui() once per frame after the 3D scene is drawn, any
 * number of render_draw_text() calls, then render_end_ui() before
 * render_end_frame().
 *
 * Coordinates are in pixels, origin top-left, matching framebuffer size
 * (the same fb_width/fb_height passed to render_init/render_set_viewport).
 * `scale` is the pixel size of one font dot (e.g. 2.0 -> a 10x14px glyph).
 */
void render_begin_ui(void);
void render_end_ui(void);
void render_draw_text(float x, float y, float scale, float r, float g, float b, const char *text);

#endif /* RENDER_H */
