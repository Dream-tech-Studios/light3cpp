/*
 * render_gx.c - GameCube renderer using libogc's GX fixed-function pipeline.
 *
 * Mirrors render_gl.c's structure and behavior as closely as GX allows:
 * unlit, per-vertex colored triangles/quads with the same CPU-side
 * directional shading formula, so the two backends look the same.
 *
 * Model matrices (translate * rotate_y * scale) are built with the exact
 * same math3d.h functions render_gl.c uses, then converted to GX's Mtx
 * format. This guarantees identical rotation direction/behavior on both
 * platforms instead of trusting GX's own guMtxRotAxisDeg to happen to
 * agree with mat4_rotate_y's convention. Only the camera (view + GX_
 * projection) uses libogc's own guLookAt/guPerspective, since those are
 * the pairing GX_LoadProjectionMtx(..., GX_PERSPECTIVE) is documented and
 * tested to expect.
 *
 * GX_SetVtxDesc/GX_SetVtxAttrFmt/GX_SetCullMode/etc. (the one-time pipeline
 * setup) live in main_gc.c, alongside VIDEO/GX_Init - the console
 * equivalent of "create the GL context" on desktop, which similarly lives
 * in main.c rather than render_gl.c.
 */
#ifdef PLATFORM_GC

#include <malloc.h>
#include <gccore.h>
#include "render.h"
#include "render_gx.h"
#include "font.h"

/* Heap-allocated pair GX needs for a bound texture: the GXTexObj header
   plus its 32-byte-aligned tiled pixel data (GX reads main memory
   directly, so the data must be flushed from CPU cache after writing and
   never freed/moved while GX might still be using it). Texture.gx_handle
   points at one of these; render_gl.c never looks at it. */
typedef struct {
    GXTexObj      obj;
    unsigned char *tiled;
} GxTextureHandle;

static int g_fb_w = 1, g_fb_h = 1;
static Mtx g_view;

static GXRModeObj *g_rmode = NULL;
static void       *g_fb[2] = { NULL, NULL };
static int         g_fb_idx = 0;

void render_gx_set_target(GXRModeObj *rmode, void *fb0, void *fb1) {
    g_rmode  = rmode;
    g_fb[0]  = fb0;
    g_fb[1]  = fb1;
    g_fb_idx = 0;
}

void render_init(int fb_width, int fb_height) {
    g_fb_w = fb_width;
    g_fb_h = fb_height;
}

void render_shutdown(void) {
}

void render_set_viewport(int fb_width, int fb_height) {
    g_fb_w = fb_width;
    g_fb_h = fb_height < 1 ? 1 : fb_height;
    /* The GameCube has no resizable window - GX_SetViewport is configured
       once in main_gc.c at startup. Just remember the aspect ratio. */
}

void render_begin_frame(void) {
    /* GX clears the EFB as part of the GX_CopyDisp call in
       render_end_frame (see GX_SetCopyClear in main_gc.c), so there is
       nothing to do at the start of a frame. */
}

void render_end_frame(void) {
    GX_DrawDone();
    g_fb_idx ^= 1;
    GX_CopyDisp(g_fb[g_fb_idx], GX_TRUE);
    VIDEO_SetNextFramebuffer(g_fb[g_fb_idx]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

void render_set_camera(Vec3 eye, Vec3 target, Vec3 up, float fov_deg) {
    guVector cam  = { eye.x, eye.y, eye.z };
    guVector upv  = { up.x, up.y, up.z };
    guVector look = { target.x, target.y, target.z };
    Mtx44 proj;
    float aspect = (float)g_fb_w / (float)g_fb_h;

    guLookAt(g_view, &cam, &upv, &look);
    guPerspective(proj, fov_deg, aspect, 0.05f, 500.0f);
    GX_LoadProjectionMtx(proj, GX_PERSPECTIVE);
}

/* math3d's Mat4 is column-major (m[col][row]); GX's Mtx is a row-major 3x4
   affine matrix (out[row][col]). The dropped fourth row is always
   [0,0,0,1] for the translate/rotate/scale matrices built here. */
static void mat4_to_gx(const Mat4 *m, Mtx out) {
    int r, c;
    for (r = 0; r < 3; ++r)
        for (c = 0; c < 4; ++c)
            out[r][c] = m->m[c][r];
}

static void load_model_matrix(Vec3 pos, float yaw, float scale) {
    Mat4 model = mat4_mul(mat4_translate(pos),
                 mat4_mul(mat4_rotate_y(yaw),
                          mat4_scale(vec3(scale, scale, scale))));
    Mtx gx_model, modelview;
    mat4_to_gx(&model, gx_model);
    guMtxConcat(g_view, gx_model, modelview);
    GX_LoadPosMtxImm(modelview, GX_PNMTX0);
}

/* Rotate a normal by the model's yaw only (translation/scale must not
   affect normals). Same formula as render_gl.c so both backends light
   meshes identically. */
static Vec3 rotate_normal_y(Vec3 n, float yaw) {
    float c = cosf(yaw), s = sinf(yaw);
    return vec3(n.x * c + n.z * s, n.y, -n.x * s + n.z * c);
}

static void gx_color_shaded(float r, float g, float b, float shade) {
    GX_Color4u8((u8)(r * shade * 255.0f), (u8)(g * shade * 255.0f),
               (u8)(b * shade * 255.0f), 255);
}

/* Converts a linear top-to-bottom RGBA8 image into GX's native RGBA8
   texture layout: 4x4 pixel tiles, each tile stored as two 32-byte
   "cache lines" - the first holding (A,R) byte pairs and the second
   holding (G,B) byte pairs, both in row-major order within the tile.
   `dw`/`dh` (the destination/padded size) must already be multiples of 4;
   source pixels past `sw`/`sh` (if padding was needed) come out transparent
   black. See the GX texture format reference on the WiiBrew wiki. */
static void gx_tile_rgba8(const unsigned char *src, int sw, int sh,
                          unsigned char *dst, int dw, int dh) {
    int bx, by, x, y;
    for (by = 0; by < dh; by += 4) {
        for (bx = 0; bx < dw; bx += 4) {
            unsigned char *tile = dst + ((size_t)(by / 4) * (size_t)(dw / 4) + (size_t)(bx / 4)) * 64;
            unsigned char *ar = tile;
            unsigned char *gb = tile + 32;
            int idx = 0;
            for (y = 0; y < 4; ++y) {
                for (x = 0; x < 4; ++x, ++idx) {
                    int sx = bx + x, sy = by + y;
                    unsigned char r = 0, g = 0, b = 0, a = 0;
                    if (sx < sw && sy < sh) {
                        const unsigned char *p = src + ((size_t)sy * (size_t)sw + (size_t)sx) * 4;
                        r = p[0]; g = p[1]; b = p[2]; a = p[3];
                    }
                    ar[idx * 2 + 0] = a; ar[idx * 2 + 1] = r;
                    gb[idx * 2 + 0] = g; gb[idx * 2 + 1] = b;
                }
            }
        }
    }
}

static int round_up4(int v) { return (v + 3) & ~3; }

int render_texture_create(Texture *tex, const unsigned char *rgba, int width, int height) {
    int dw = round_up4(width), dh = round_up4(height);
    size_t tiled_size = (size_t)(dw / 4) * (size_t)(dh / 4) * 64;
    GxTextureHandle *h = (GxTextureHandle *)malloc(sizeof(GxTextureHandle));

    h->tiled = (unsigned char *)memalign(32, tiled_size);
    gx_tile_rgba8(rgba, width, height, h->tiled, dw, dh);
    DCFlushRange(h->tiled, tiled_size);

    /* Some meshes (e.g. the bike) author UVs outside [0,1] expecting the
       texture to tile, so wrap rather than clamp (see the u_scale/v_scale
       comment in render_draw_mesh() for the one case - a non-multiple-of-4
       sized texture - where padding and wrapping don't perfectly combine). */
    GX_InitTexObj(&h->obj, h->tiled, (u16)dw, (u16)dh, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, GX_FALSE);
    GX_InitTexObjFilterMode(&h->obj, GX_LINEAR, GX_LINEAR);

    tex->loaded    = 1;
    tex->width     = width;
    tex->height    = height;
    tex->gl_id     = 0;
    tex->gx_handle = h;
    return 1;
}

void render_texture_destroy(Texture *tex) {
    GxTextureHandle *h;
    if (!tex->loaded) return;
    h = (GxTextureHandle *)tex->gx_handle;
    free(h->tiled);
    free(h);
    tex->loaded = 0;
    tex->gx_handle = NULL;
}

void render_draw_mesh(const Mesh *m, Vec3 pos, float yaw, float scale, const Texture *tex) {
    Vec3 L = vec3_norm(vec3(0.4f, 0.85f, 0.35f));
    int textured = (tex && tex->loaded && tex->gx_handle);
    int i;

    if (textured) {
        GxTextureHandle *h = (GxTextureHandle *)tex->gx_handle;
        /* Padded (out to a multiple of 4) vs. the image's real size means
           u=1/v=1 must land on the real last pixel, not the padding. This
           is exact when width/height are already multiples of 4 (no
           padding, scale=1) - true for every texture baked so far - but
           an unpadded texture that also relies on wrapping (u/v outside
           [0,1], see GX_REPEAT above) would show a seam at the padded
           boundary instead of the intended one; keep baked textures
           sized to a multiple of 4 to avoid that. */
        float u_scale = (float)tex->width  / (float)round_up4(tex->width);
        float v_scale = (float)tex->height / (float)round_up4(tex->height);

        GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        GX_SetNumTexGens(1);
        GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
        GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
        GX_LoadTexObj(&h->obj, GX_TEXMAP0);

        load_model_matrix(pos, yaw, scale);
        GX_Begin(GX_TRIANGLES, GX_VTXFMT1, (u16)m->vert_count);
        for (i = 0; i < m->vert_count; ++i) {
            const MeshVertex *v = &m->verts[i];
            Vec3  wn = rotate_normal_y(v->normal, yaw);
            float d  = vec3_dot(wn, L);
            float shade;
            if (d < 0.0f) d = 0.0f;
            shade = 0.35f + 0.65f * d;

            GX_Position3f32(v->pos.x, v->pos.y, v->pos.z);
            /* GX_MODULATE multiplies the texel by this vertex color, so a
               flat gray reproduces the same directional shading the
               untextured path bakes into mt->r/g/b. */
            gx_color_shaded(1.0f, 1.0f, 1.0f, shade);
            GX_TexCoord2f32(v->u * u_scale, v->v * v_scale);
        }
        GX_End();

        /* Restore the untextured pipeline main_gc.c set up, since
           draw_ground/draw_box/hud text all reuse GX_VTXFMT0 without
           texcoords. */
        GX_SetNumTexGens(0);
        GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
        GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
    } else {
        load_model_matrix(pos, yaw, scale);
        GX_Begin(GX_TRIANGLES, GX_VTXFMT0, (u16)m->vert_count);
        for (i = 0; i < m->vert_count; ++i) {
            const MeshVertex   *v  = &m->verts[i];
            const MeshMaterial *mt = &m->materials[v->mtl];
            Vec3  wn = rotate_normal_y(v->normal, yaw);
            float d  = vec3_dot(wn, L);
            float shade;
            if (d < 0.0f) d = 0.0f;
            shade = 0.35f + 0.65f * d;

            GX_Position3f32(v->pos.x, v->pos.y, v->pos.z);
            gx_color_shaded(mt->r, mt->g, mt->b, shade);
        }
        GX_End();
    }
}

void render_draw_box(Vec3 pos, Vec3 he, float yaw, float r, float g, float b) {
    load_model_matrix(pos, yaw, 1.0f);

    GX_Begin(GX_QUADS, GX_VTXFMT0, 24);

    /* front (+Z) */
    GX_Position3f32(-he.x, -he.y,  he.z); gx_color_shaded(r, g, b, 1.0f);
    GX_Position3f32( he.x, -he.y,  he.z); gx_color_shaded(r, g, b, 1.0f);
    GX_Position3f32( he.x,  he.y,  he.z); gx_color_shaded(r, g, b, 1.0f);
    GX_Position3f32(-he.x,  he.y,  he.z); gx_color_shaded(r, g, b, 1.0f);
    /* back (-Z) */
    GX_Position3f32( he.x, -he.y, -he.z); gx_color_shaded(r, g, b, 0.8f);
    GX_Position3f32(-he.x, -he.y, -he.z); gx_color_shaded(r, g, b, 0.8f);
    GX_Position3f32(-he.x,  he.y, -he.z); gx_color_shaded(r, g, b, 0.8f);
    GX_Position3f32( he.x,  he.y, -he.z); gx_color_shaded(r, g, b, 0.8f);
    /* left (-X) */
    GX_Position3f32(-he.x, -he.y, -he.z); gx_color_shaded(r, g, b, 0.7f);
    GX_Position3f32(-he.x, -he.y,  he.z); gx_color_shaded(r, g, b, 0.7f);
    GX_Position3f32(-he.x,  he.y,  he.z); gx_color_shaded(r, g, b, 0.7f);
    GX_Position3f32(-he.x,  he.y, -he.z); gx_color_shaded(r, g, b, 0.7f);
    /* right (+X) */
    GX_Position3f32( he.x, -he.y,  he.z); gx_color_shaded(r, g, b, 0.7f);
    GX_Position3f32( he.x, -he.y, -he.z); gx_color_shaded(r, g, b, 0.7f);
    GX_Position3f32( he.x,  he.y, -he.z); gx_color_shaded(r, g, b, 0.7f);
    GX_Position3f32( he.x,  he.y,  he.z); gx_color_shaded(r, g, b, 0.7f);
    /* top (+Y) */
    GX_Position3f32(-he.x,  he.y,  he.z); gx_color_shaded(r, g, b, 1.0f);
    GX_Position3f32( he.x,  he.y,  he.z); gx_color_shaded(r, g, b, 1.0f);
    GX_Position3f32( he.x,  he.y, -he.z); gx_color_shaded(r, g, b, 1.0f);
    GX_Position3f32(-he.x,  he.y, -he.z); gx_color_shaded(r, g, b, 1.0f);
    /* bottom (-Y) */
    GX_Position3f32(-he.x, -he.y, -he.z); gx_color_shaded(r, g, b, 0.5f);
    GX_Position3f32( he.x, -he.y, -he.z); gx_color_shaded(r, g, b, 0.5f);
    GX_Position3f32( he.x, -he.y,  he.z); gx_color_shaded(r, g, b, 0.5f);
    GX_Position3f32(-he.x, -he.y,  he.z); gx_color_shaded(r, g, b, 0.5f);

    GX_End();
}

void render_draw_ground(float y, float half_size, float tile) {
    int n = (int)(half_size / tile);
    int nx, nz;
    int side, quads;

    if (n < 1) n = 1;
    side  = 2 * n;
    quads = side * side;

    load_model_matrix(vec3_zero(), 0.0f, 1.0f); /* static geometry, identity model */

    GX_Begin(GX_QUADS, GX_VTXFMT0, (u16)(quads * 4));
    for (nx = -n; nx < n; ++nx) {
        for (nz = -n; nz < n; ++nz) {
            int   checker = (nx + nz) & 1;
            float x = (float)nx * tile;
            float z = (float)nz * tile;
            float shade = checker ? 1.0f : 0.87f;

            GX_Position3f32(x,        y, z + tile); gx_color_shaded(0.30f, 0.55f, 0.30f, shade);
            GX_Position3f32(x + tile, y, z + tile); gx_color_shaded(0.30f, 0.55f, 0.30f, shade);
            GX_Position3f32(x + tile, y, z);        gx_color_shaded(0.30f, 0.55f, 0.30f, shade);
            GX_Position3f32(x,        y, z);        gx_color_shaded(0.30f, 0.55f, 0.30f, shade);
        }
    }
    GX_End();
}

void render_begin_ui(void) {
    Mtx44 proj;
    Mtx   identity;

    /* Pixel-space, top-left origin, +Y down - matches render_gl.c's
       glOrtho(0, w, h, 0, -1, 1) overlay convention. */
    guOrtho(proj, 0.0f, (f32)g_fb_h, 0.0f, (f32)g_fb_w, -1.0f, 1.0f);
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);

    guMtxIdentity(identity);
    GX_LoadPosMtxImm(identity, GX_PNMTX0);

    /* Draw the HUD on top of the 3D scene regardless of depth; 3D drawing
       (render_set_camera + load_model_matrix) restores its own projection
       and Z state at the start of the next frame. */
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
}

void render_end_ui(void) {
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
}

static void gx_color_flat(float r, float g, float b) {
    GX_Color4u8((u8)(r * 255.0f), (u8)(g * 255.0f), (u8)(b * 255.0f), 255);
}

static int count_lit_bits(unsigned char row) {
    int i, n = 0;
    for (i = 0; i < FONT_GLYPH_W; ++i) {
        if ((row >> i) & 1) ++n;
    }
    return n;
}

void render_draw_text(float x, float y, float scale, float r, float g, float b, const char *text) {
    const char *p;
    int lit = 0;

    for (p = text; *p; ++p) {
        const unsigned char *rows = font_glyph_rows(*p);
        int row;
        for (row = 0; row < FONT_GLYPH_H; ++row) lit += count_lit_bits(rows[row]);
    }
    if (lit == 0) return;

    GX_Begin(GX_QUADS, GX_VTXFMT0, (u16)(lit * 4));
    {
        float cx = x;
        for (p = text; *p; ++p) {
            const unsigned char *rows = font_glyph_rows(*p);
            int row, col;
            for (row = 0; row < FONT_GLYPH_H; ++row) {
                for (col = 0; col < FONT_GLYPH_W; ++col) {
                    float px, py;
                    if (!((rows[row] >> (FONT_GLYPH_W - 1 - col)) & 1)) continue;
                    px = cx + (float)col * scale;
                    py = y + (float)row * scale;

                    GX_Position3f32(px,         py,         0.0f); gx_color_flat(r, g, b);
                    GX_Position3f32(px + scale, py,         0.0f); gx_color_flat(r, g, b);
                    GX_Position3f32(px + scale, py + scale, 0.0f); gx_color_flat(r, g, b);
                    GX_Position3f32(px,         py + scale, 0.0f); gx_color_flat(r, g, b);
                }
            }
            cx += (float)(FONT_GLYPH_W + 1) * scale;
        }
    }
    GX_End();
}

#endif /* PLATFORM_GC */
