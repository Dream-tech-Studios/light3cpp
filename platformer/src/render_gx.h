/*
 * render_gx.h - extra GameCube-only hookup between main_gc.c and render_gx.c.
 *
 * render.h stays platform-agnostic (it's included by game.c, which must
 * compile unchanged on desktop too). But render_gx.c's render_end_frame()
 * needs to know which two framebuffers to flip between and the render mode
 * object describing the current video mode - things a windowing system
 * like SDL2 would normally hide from render_gl.c. main_gc.c hands them
 * over once at startup via this small side-channel.
 */
#ifndef RENDER_GX_H
#define RENDER_GX_H

#include <gccore.h>

void render_gx_set_target(GXRModeObj *rmode, void *fb0, void *fb1);

#endif /* RENDER_GX_H */
