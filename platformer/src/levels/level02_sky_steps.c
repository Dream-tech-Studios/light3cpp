/*
 * level02_sky_steps.c - see level02_sky_steps.h.
 *
 * Same viewed-from-above convention as level01 (+X right, -Z "forward"),
 * just a longer run: six climbing steps instead of four, a plateau to
 * catch your breath, then a steep two-stage drop to the goal.
 *
 *   start (z=26) -> ... -> goal (z=-40)
 */
#include "level02_sky_steps.h"

void level02_sky_steps_build(Level *lvl) {
    level_begin(lvl, "2. Sky Steps");

    /* Start plaza. */
    level_add_platform(lvl, vec3(0.0f, 0.0f, 24.0f), 6.0f, 6.0f, 1.0f);

    /* A flat hop to get moving. */
    level_add_platform(lvl, vec3(0.0f, 0.0f, 14.0f), 2.5f, 2.0f, 1.0f);

    /* Climb - six steps this time, still 0.8u per step. */
    level_add_platform(lvl, vec3(0.0f, 0.8f,   8.0f), 2.0f, 1.8f, 0.6f);
    level_add_platform(lvl, vec3(0.0f, 1.6f,   3.0f), 1.8f, 1.8f, 0.6f);
    level_add_platform(lvl, vec3(0.0f, 2.4f,  -2.0f), 1.8f, 1.8f, 0.6f);
    level_add_platform(lvl, vec3(0.0f, 3.2f,  -7.0f), 1.8f, 1.8f, 0.6f);
    level_add_platform(lvl, vec3(0.0f, 4.0f, -12.0f), 2.0f, 2.0f, 0.6f);

    /* Breather plateau - same height as the last step, extra-wide. */
    level_add_platform(lvl, vec3(0.0f, 4.0f, -18.0f), 2.2f, 2.2f, 0.6f);

    /* Steep two-stage drop back down toward the goal. */
    level_add_platform(lvl, vec3(0.0f, 2.0f, -24.0f), 2.4f, 2.4f, 0.6f);
    level_add_platform(lvl, vec3(0.0f, 0.0f, -30.0f), 3.0f, 3.0f, 0.6f);

    /* Goal plaza. */
    level_add_platform(lvl, vec3(0.0f, 0.0f, -40.0f), 7.0f, 7.0f, 1.0f);

    /* Collectibles: a freebie on the start plaza, then one over most steps. */
    level_add_collectible(lvl, vec3(4.0f, 1.0f,  24.0f));
    level_add_collectible(lvl, vec3(0.0f, 1.6f,  14.0f));
    level_add_collectible(lvl, vec3(0.0f, 2.4f,   8.0f));
    level_add_collectible(lvl, vec3(0.0f, 3.2f,   3.0f));
    level_add_collectible(lvl, vec3(0.0f, 4.0f,  -2.0f));
    level_add_collectible(lvl, vec3(0.0f, 4.8f,  -7.0f));
    level_add_collectible(lvl, vec3(0.0f, 5.6f, -12.0f));
    level_add_collectible(lvl, vec3(0.0f, 5.6f, -18.0f));
    level_add_collectible(lvl, vec3(0.0f, 3.6f, -24.0f));

    level_set_start(lvl, vec3(0.0f, 0.0f, 26.0f), 0.0f);
    level_set_goal(lvl, vec3(0.0f, 1.0f, -40.0f), 3.5f);
}
