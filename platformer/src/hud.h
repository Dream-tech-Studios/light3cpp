/*
 * hud.h - on-screen HUD layout: collectible counter, timer, "ready"/"GO!"
 * and level-complete banners. Pure presentation on top of an Objective - no
 * game/input logic lives here.
 */
#ifndef HUD_H
#define HUD_H

#include "objective.h"

/*
 * Draws the full HUD for the current frame. Must be called between the 3D
 * scene draw and render_end_frame(); internally wraps its drawing in
 * render_begin_ui()/render_end_ui().
 *
 * `level_index`/`level_count` (0-based index into g_levels[], and
 * LEVEL_COUNT - see levels.h) are only used to label the current level and
 * to word the "what happens next" hint on the completion screen; hud.c
 * otherwise knows nothing about level progression/selection.
 */
void hud_render(const Objective *objective, int level_index, int level_count, int fb_w, int fb_h);

#endif /* HUD_H */
