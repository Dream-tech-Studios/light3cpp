/*
 * hud.h - on-screen HUD layout: lap counter, position, timer, countdown
 * and finish banner. Pure presentation on top of a Race - no game/input
 * logic lives here.
 */
#ifndef HUD_H
#define HUD_H

#include "race.h"

/* Draws the full HUD for the current frame. Must be called between the 3D
   scene draw and render_end_frame(); internally wraps its drawing in
   render_begin_ui()/render_end_ui(). */
void hud_render(const Race *race, int fb_w, int fb_h);

#endif /* HUD_H */
