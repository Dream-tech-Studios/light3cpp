/*
 * carousel.h - a reusable "spin through options and pick one" selector,
 * used for the character select screen.
 *
 * It owns no assets: callers hand it an array of CarouselItem (each pointing
 * at an already-loaded preview model plus display text/stats), and it handles
 * left/right navigation (with its own press-vs-hold edge detection so one
 * press = one step), spins the current model, and renders the whole screen
 * (title, name, arrows, ASCII stat bars, hint). It reports CONFIRM/BACK when
 * the player commits, and the caller reads carousel_index() for the pick.
 */
#ifndef CAROUSEL_H
#define CAROUSEL_H

#include "input.h"
#include "model_load.h"

#define CAROUSEL_MAX_STATS 4
#define CAROUSEL_STAT_LABEL 12

/* One 0..10 stat bar. `present` = 0 renders as "--" (for an optional stat
   an item's JSON entry left null/absent). Unused today (characters have no
   stats), but kept for any future item that wants one. */
typedef struct {
    char  label[CAROUSEL_STAT_LABEL];
    float value;
    int   present;
} CarouselStat;

typedef struct {
    const char          *name;
    const ModelInstance *model;   /* borrowed preview model; may be NULL / has_mesh=0 */
    CarouselStat         stats[CAROUSEL_MAX_STATS];
    int                  stat_count;
} CarouselItem;

typedef enum {
    CAROUSEL_NONE,
    CAROUSEL_CONFIRM,
    CAROUSEL_BACK
} CarouselResult;

typedef struct {
    const char         *title;
    const CarouselItem *items;   /* borrowed */
    int                 count;
    int                 index;
    int                 allow_back;
    float               spin;    /* preview yaw, advances over time */

    /* previous-frame button state for edge detection */
    int prev_left, prev_right, prev_confirm, prev_back;
} Carousel;

void           carousel_init(Carousel *c, const char *title,
                             const CarouselItem *items, int count,
                             int start_index, int allow_back);
CarouselResult carousel_step(Carousel *c, const PlayerInput *in, float dt);
void           carousel_render(const Carousel *c, int fb_w, int fb_h);

static inline int carousel_index(const Carousel *c) { return c->index; }

#endif /* CAROUSEL_H */
