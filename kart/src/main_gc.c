/*
 * main_gc.c - GameCube platform layer (PLATFORM_GC), built with devkitPPC
 * against libogc.
 *
 * Owns VIDEO/GX bootstrap and the double-buffered framebuffer, plus the
 * main loop - the console equivalent of what main.c does with SDL2's
 * window/GL context on desktop. This is the one-time GX pipeline setup
 * (video mode, vertex format, disabled lighting/texturing since this
 * renderer is unlit per-vertex color only); the per-frame drawing lives in
 * render_gx.c behind the same render.h interface render_gl.c implements.
 *
 * There's no real clock to read on bare hardware without extra setup, so
 * the simulation timestep is derived from the detected TV standard (60Hz
 * NTSC/MPAL, 50Hz PAL) and the loop is paced by VIDEO_WaitVSync (called
 * inside render_end_frame), exactly one physics step per displayed frame.
 */
#ifdef PLATFORM_GC

#include <malloc.h>
#include <string.h>
#include <gccore.h>
#include "app.h"
#include "gcdvd.h"
#include "input.h"
#include "render.h"
#include "render_gx.h"

#define DEFAULT_FIFO_SIZE (256 * 1024)

static void *frame_buffer[2] = { NULL, NULL };

int main(int argc, char **argv) {
    GXRModeObj *rmode;
    void *gp_fifo;
    App app;
    KartInput in;
    GXColor bg = { 135, 206, 235, 255 }; /* sky blue, matches render_gl.c */
    float fixed_dt;

    (void)argc; (void)argv;

    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);

    frame_buffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    frame_buffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(frame_buffer[0]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();

    gp_fifo = memalign(32, DEFAULT_FIFO_SIZE);
    memset(gp_fifo, 0, DEFAULT_FIFO_SIZE);
    GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);

    GX_SetCopyClear(bg, 0x00ffffff);

    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    {
        f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
        u32 xfb_height = GX_SetDispCopyYScale(yscale);
        GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
        GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
        GX_SetDispCopyDst(rmode->fbWidth, (u16)xfb_height);
    }
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering,
                    ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
    GX_SetDispCopyGamma(GX_GM_1_0);

    /* Backface culling off: same reasoning as render_gl.c - loaded/baked
       OBJ meshes aren't guaranteed consistent winding, and this game only
       draws a handful of small meshes, so it's not worth the risk. */
    GX_SetCullMode(GX_CULL_NONE);

    /* Unlit, direct (non-indexed) position + per-vertex color pipeline -
       matches render_gl.c's plain vertex-color immediate mode with no
       hardware lighting. GX_VA_TEX0 starts as GX_NONE (untextured); the one
       place that needs texturing (render_draw_mesh in render_gx.c) turns it
       on for just that draw call and turns it back off afterward, since
       every other draw call (ground/walls/HUD text) still uses VTXFMT0
       with no texture coordinates. */
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    /* Second vertex format, used only for textured mesh draws: same
       position/color layout plus a texture coordinate. */
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    GX_SetNumChans(1);
    GX_SetNumTexGens(0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);

    render_gx_set_target(rmode, frame_buffer[0], frame_buffer[1]);

    /* Best-effort: if this .dol was booted standalone (no disc, or a disc
       with no FST/assets staged next to it), this just fails and asset
       loading below falls back to placeholders, same as desktop running with
       no assets/ directory. */
    gcdvd_mount();

    input_init();
    app_init(&app, rmode->fbWidth, rmode->efbHeight);

    fixed_dt = ((rmode->viTVMode >> 2) == VI_PAL) ? (1.0f / 50.0f) : (1.0f / 60.0f);

    in.quit = 0;
    for (;;) {
        input_poll(&in); /* never sets quit on GC; runs until power-off/reset */
        app_step(&app, &in, fixed_dt);
        app_render(&app, rmode->fbWidth, rmode->efbHeight); /* render_end_frame() waits for vsync */
    }

    /* Unreachable on real hardware, kept for symmetry with main.c. */
    app_shutdown(&app);
    input_shutdown();
    return 0;
}

#endif /* PLATFORM_GC */
