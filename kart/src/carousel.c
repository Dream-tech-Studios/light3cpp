/*
 * carousel.c - see carousel.h.
 */
#include <stdio.h>
#include <string.h>
#include "carousel.h"
#include "render.h"
#include "math3d.h"
#include "font.h"

/* Preview camera: a fixed 3/4 view of a model standing on the ground at the
   origin (models are recentered so their base sits at y=0). */
#define PREVIEW_SPIN_RATE 0.9f   /* rad/s */
#define UI_SCALE          2.0f
#define UI_TITLE_SCALE    3.0f
#define UI_MARGIN         12.0f
#define STAT_BAR_CELLS    10

static float text_width(const char *s, float scale) {
    return (float)strlen(s) * (float)(FONT_GLYPH_W + 1) * scale;
}

static float glyph_h(float scale) { return (float)FONT_GLYPH_H * scale; }

void carousel_init(Carousel *c, const char *title,
                   const CarouselItem *items, int count,
                   int start_index, int allow_back) {
    c->title      = title;
    c->items      = items;
    c->count      = count;
    c->index      = (count > 0) ? (start_index % count) : 0;
    if (c->index < 0) c->index = 0;
    c->allow_back = allow_back;
    c->spin       = 0.0f;
    c->prev_left  = c->prev_right = 0;
    /* Assume confirm/back may still be held from whatever transition brought
       us here (e.g. the button press that left the previous scene), so a
       fresh press is required before they fire - no accidental double-skip. */
    c->prev_confirm = c->prev_back = 1;
}

CarouselResult carousel_step(Carousel *c, const KartInput *in, float dt) {
    CarouselResult result = CAROUSEL_NONE;

    c->spin += PREVIEW_SPIN_RATE * dt;

    if (c->count > 0) {
        if (in->menu_left && !c->prev_left)
            c->index = (c->index - 1 + c->count) % c->count;
        if (in->menu_right && !c->prev_right)
            c->index = (c->index + 1) % c->count;
    }

    if (in->menu_confirm && !c->prev_confirm)
        result = CAROUSEL_CONFIRM;
    else if (c->allow_back && in->menu_back && !c->prev_back)
        result = CAROUSEL_BACK;

    c->prev_left    = in->menu_left;
    c->prev_right   = in->menu_right;
    c->prev_confirm = in->menu_confirm;
    c->prev_back    = in->menu_back;

    return result;
}

static void draw_preview(const CarouselItem *it, float spin) {
    if (it && it->model && it->model->has_mesh) {
        const ModelInstance *m = it->model;
        render_draw_mesh(&m->mesh, vec3(0.0f, 0.0f, 0.0f), m->yaw_off + spin,
                         m->scale, m->has_texture ? &m->texture : NULL);
    } else {
        /* Missing preview model: spin a placeholder box so the slot still
           reads as "something is selected here". */
        render_draw_box(vec3(0.0f, 0.6f, 0.0f), vec3(0.5f, 0.6f, 0.5f), spin,
                        0.85f, 0.2f, 0.2f);
    }
}

static void draw_stat_bar(float x, float y, const CarouselStat *st) {
    char bar[STAT_BAR_CELLS + 4];
    char line[CAROUSEL_STAT_LABEL + STAT_BAR_CELLS + 8];

    if (st->present) {
        int filled = (int)(st->value + 0.5f);
        int i, k = 0;
        if (filled < 0) filled = 0;
        if (filled > STAT_BAR_CELLS) filled = STAT_BAR_CELLS;
        bar[k++] = '[';
        for (i = 0; i < STAT_BAR_CELLS; ++i) bar[k++] = (i < filled) ? '#' : '-';
        bar[k++] = ']';
        bar[k] = '\0';
    } else {
        strcpy(bar, "[--------]");
    }

    snprintf(line, sizeof(line), "%-11s %s", st->label, bar);
    render_draw_text(x, y, UI_SCALE, 1.0f, 1.0f, 1.0f, line);
}

void carousel_render(const Carousel *c, int fb_w, int fb_h) {
    const CarouselItem *it = (c->count > 0) ? &c->items[c->index] : NULL;

    render_begin_frame();

    /* 3/4 view of the model standing at the origin. */
    render_set_camera(vec3(0.0f, 1.5f, 4.6f), vec3(0.0f, 0.75f, 0.0f),
                      vec3(0, 1, 0), 50.0f);
    render_draw_ground(0.0f, 8.0f, 2.0f);
    draw_preview(it, c->spin);

    render_begin_ui();

    /* Title, centered near the top. */
    render_draw_text(((float)fb_w - text_width(c->title, UI_TITLE_SCALE)) * 0.5f,
                     UI_MARGIN, UI_TITLE_SCALE, 1.0f, 1.0f, 0.2f, c->title);

    if (it) {
        /* Selected name, centered just below the title. */
        float name_y = UI_MARGIN + glyph_h(UI_TITLE_SCALE) + 10.0f;
        render_draw_text(((float)fb_w - text_width(it->name, UI_SCALE * 1.5f)) * 0.5f,
                         name_y, UI_SCALE * 1.5f, 1.0f, 1.0f, 1.0f, it->name);

        /* Left / right arrows, vertically centered at the screen edges. */
        {
            float arrow_y = (float)fb_h * 0.5f;
            render_draw_text(UI_MARGIN, arrow_y, UI_TITLE_SCALE, 1.0f, 1.0f, 1.0f, "<");
            render_draw_text((float)fb_w - UI_MARGIN - text_width(">", UI_TITLE_SCALE),
                             arrow_y, UI_TITLE_SCALE, 1.0f, 1.0f, 1.0f, ">");
        }

        /* Stat bars, stacked in the lower-left. */
        {
            int i;
            float line_h = glyph_h(UI_SCALE) + 6.0f;
            float sy = (float)fb_h - UI_MARGIN - line_h * (float)(it->stat_count + 1);
            for (i = 0; i < it->stat_count; ++i)
                draw_stat_bar(UI_MARGIN, sy + line_h * (float)i, &it->stats[i]);
        }
    }

    /* Controls hint, bottom center. */
    {
        const char *hint = c->allow_back ? "< > CHOOSE   A / ENTER SELECT   B / BKSP BACK"
                                          : "< > CHOOSE   A / ENTER SELECT";
        render_draw_text(((float)fb_w - text_width(hint, UI_SCALE)) * 0.5f,
                         (float)fb_h - UI_MARGIN - glyph_h(UI_SCALE),
                         UI_SCALE, 0.8f, 0.8f, 0.8f, hint);
    }

    render_end_ui();
    render_end_frame();
}
