/*
 * hud.c - see hud.h.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "hud.h"
#include "render.h"
#include "font.h"

#define HUD_SCALE     2.0f
#define HUD_BIG_SCALE 6.0f
#define HUD_MARGIN    10.0f

static float text_width(const char *s, float scale) {
    return (float)strlen(s) * (float)(FONT_GLYPH_W + 1) * scale;
}

static void format_time(float seconds, char *out, size_t out_size) {
    int mins, secs, tenths;
    if (seconds < 0.0f) seconds = 0.0f;
    mins   = (int)(seconds / 60.0f);
    secs   = (int)seconds % 60;
    tenths = (int)(seconds * 10.0f) % 10;
    snprintf(out, out_size, "%02d:%02d.%d", mins, secs, tenths);
}

void hud_render(const Objective *objective, int level_index, int level_count, int fb_w, int fb_h) {
    char buf[48];
    int has_next_level = (level_index + 1 < level_count);

    render_begin_ui();

    /* Collectible counter, top-left */
    snprintf(buf, sizeof(buf), "STARS %d/%d", objective->collected, objective->level->collectible_count);
    render_draw_text(HUD_MARGIN, HUD_MARGIN, HUD_SCALE, 1.0f, 0.85f, 0.2f, buf);

    /* Level name, top-right (e.g. "1. First Steps" - see level_begin). */
    render_draw_text((float)fb_w - HUD_MARGIN - text_width(objective->level->name, HUD_SCALE),
                      HUD_MARGIN, HUD_SCALE, 0.85f, 0.85f, 1.0f, objective->level->name);

    /* Timer, top-center - counts up while PLAYING, frozen otherwise since
       objective.c only advances play_time in OBJECTIVE_PLAYING. */
    format_time(objective->play_time, buf, sizeof(buf));
    render_draw_text(((float)fb_w - text_width(buf, HUD_SCALE)) * 0.5f, HUD_MARGIN,
                      HUD_SCALE, 1.0f, 1.0f, 1.0f, buf);

    if (objective->state == OBJECTIVE_READY) {
        int n = (int)ceilf(objective->state_timer);
        if (n < 1) n = 1;
        snprintf(buf, sizeof(buf), "%d", n);
        render_draw_text(((float)fb_w - text_width(buf, HUD_BIG_SCALE)) * 0.5f,
                          (float)fb_h * 0.4f, HUD_BIG_SCALE, 1.0f, 1.0f, 0.2f, buf);
    } else if (objective->state == OBJECTIVE_PLAYING && objective->play_time < OBJECTIVE_GO_FLASH_SECONDS) {
        render_draw_text(((float)fb_w - text_width("GO!", HUD_BIG_SCALE)) * 0.5f,
                          (float)fb_h * 0.4f, HUD_BIG_SCALE, 0.3f, 1.0f, 0.3f, "GO!");
    } else if (objective->state == OBJECTIVE_COMPLETE) {
        const char *line1 = "LEVEL COMPLETE!";
        const char *hint  = has_next_level ? "PRESS A / ENTER FOR THE NEXT LEVEL"
                                            : "PRESS A / ENTER TO CONTINUE";
        float line1_y = (float)fb_h * 0.32f;
        float time_y  = line1_y + HUD_BIG_SCALE * (float)FONT_GLYPH_H + 16.0f;
        float stars_y = time_y + HUD_SCALE * 1.5f * (float)FONT_GLYPH_H + 12.0f;

        format_time(objective->play_time, buf, sizeof(buf));

        render_draw_text(((float)fb_w - text_width(line1, HUD_BIG_SCALE)) * 0.5f,
                          line1_y, HUD_BIG_SCALE, 1.0f, 1.0f, 0.2f, line1);
        render_draw_text(((float)fb_w - text_width(buf, HUD_SCALE * 1.5f)) * 0.5f,
                          time_y, HUD_SCALE * 1.5f, 1.0f, 1.0f, 1.0f, buf);

        snprintf(buf, sizeof(buf), "STARS %d/%d", objective->collected, objective->level->collectible_count);
        render_draw_text(((float)fb_w - text_width(buf, HUD_SCALE)) * 0.5f,
                          stars_y, HUD_SCALE, 1.0f, 0.85f, 0.2f, buf);

        render_draw_text(((float)fb_w - text_width(hint, HUD_SCALE)) * 0.5f,
                          (float)fb_h * 0.62f, HUD_SCALE, 0.8f, 0.8f, 0.8f, hint);
    }

    render_end_ui();
}
