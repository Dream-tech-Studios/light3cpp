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

static void ordinal_suffix(int n, char *out, size_t out_size) {
    int rem100 = n % 100;
    const char *suf = "TH";
    if (rem100 < 11 || rem100 > 13) {
        switch (n % 10) {
        case 1: suf = "ST"; break;
        case 2: suf = "ND"; break;
        case 3: suf = "RD"; break;
        default: break;
        }
    }
    snprintf(out, out_size, "%d%s", n, suf);
}

static void format_time(float seconds, char *out, size_t out_size) {
    int mins, secs, tenths;
    if (seconds < 0.0f) seconds = 0.0f;
    mins   = (int)(seconds / 60.0f);
    secs   = (int)seconds % 60;
    tenths = (int)(seconds * 10.0f) % 10;
    snprintf(out, out_size, "%02d:%02d.%d", mins, secs, tenths);
}

void hud_render(const Race *race, int fb_w, int fb_h) {
    char buf[32];

    render_begin_ui();

    /* Lap counter, top-left */
    snprintf(buf, sizeof(buf), "LAP %d/%d", race->current_lap, race->total_laps);
    render_draw_text(HUD_MARGIN, HUD_MARGIN, HUD_SCALE, 1.0f, 1.0f, 1.0f, buf);

    /* Position, top-right */
    ordinal_suffix(race_position(race), buf, sizeof(buf));
    render_draw_text((float)fb_w - HUD_MARGIN - text_width(buf, HUD_SCALE), HUD_MARGIN,
                      HUD_SCALE, 1.0f, 0.9f, 0.2f, buf);

    /* Timer, top-center - counts up while RUNNING, frozen otherwise since
       race.c only advances race_time in RACE_RUNNING. */
    format_time(race->race_time, buf, sizeof(buf));
    render_draw_text(((float)fb_w - text_width(buf, HUD_SCALE)) * 0.5f, HUD_MARGIN,
                      HUD_SCALE, 1.0f, 1.0f, 1.0f, buf);

    if (race->state == RACE_COUNTDOWN) {
        int n = (int)ceilf(race->state_timer);
        if (n < 1) n = 1;
        snprintf(buf, sizeof(buf), "%d", n);
        render_draw_text(((float)fb_w - text_width(buf, HUD_BIG_SCALE)) * 0.5f,
                          (float)fb_h * 0.4f, HUD_BIG_SCALE, 1.0f, 1.0f, 0.2f, buf);
    } else if (race->state == RACE_RUNNING && race->race_time < RACE_GO_FLASH_SECONDS) {
        render_draw_text(((float)fb_w - text_width("GO!", HUD_BIG_SCALE)) * 0.5f,
                          (float)fb_h * 0.4f, HUD_BIG_SCALE, 0.3f, 1.0f, 0.3f, "GO!");
    } else if (race->state == RACE_FINISHED) {
        const char *line1 = "FINISHED!";
        const char *hint  = "PRESS A / ENTER TO CONTINUE";
        float line1_y = (float)fb_h * 0.35f;
        float time_y  = line1_y + HUD_BIG_SCALE * (float)FONT_GLYPH_H + 16.0f;

        format_time(race->race_time, buf, sizeof(buf));

        render_draw_text(((float)fb_w - text_width(line1, HUD_BIG_SCALE)) * 0.5f,
                          line1_y, HUD_BIG_SCALE, 1.0f, 1.0f, 0.2f, line1);
        render_draw_text(((float)fb_w - text_width(buf, HUD_SCALE * 1.5f)) * 0.5f,
                          time_y, HUD_SCALE * 1.5f, 1.0f, 1.0f, 1.0f, buf);
        render_draw_text(((float)fb_w - text_width(hint, HUD_SCALE)) * 0.5f,
                          (float)fb_h * 0.6f, HUD_SCALE, 0.8f, 0.8f, 0.8f, hint);
    }

    render_end_ui();
}
