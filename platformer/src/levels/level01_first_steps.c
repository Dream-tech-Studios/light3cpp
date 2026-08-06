/*
 * level01_first_steps.c - see level01_first_steps.h.
 *
 * Layout (viewed from above, +X right, -Z "forward" - matches heading 0 in
 * physics.c): a start plaza, a chain of platforms climbing then descending
 * with gaps/turns that need a jump, and a goal plaza at the far end.
 *
 *   start (z=18) -> ... -> goal (z=-34)
 */
#include "level01_first_steps.h"

void level01_first_steps_build(Level *lvl) {
    level_begin(lvl, "1. First Steps");

    /* Start plaza (a wide, low platform - just solid ground to begin on). */
    level_add_platform(lvl, vec3(0.0f, 0.0f, 18.0f), 6.0f, 6.0f, 1.0f);

    /* A short, flat hop to get used to jumping gaps. */
    level_add_platform(lvl, vec3(0.0f, 0.0f, 8.0f), 2.5f, 2.0f, 1.0f);

    /* Climb, each step a bit higher (0.8u - well inside a single jump's
       ~2.3u apex height) and a bit further. */
    level_add_platform(lvl, vec3(0.0f, 0.8f,   2.0f), 2.0f, 1.8f, 0.6f);
    level_add_platform(lvl, vec3(0.0f, 1.6f,  -3.0f), 1.8f, 1.8f, 0.6f);
    level_add_platform(lvl, vec3(0.0f, 2.4f,  -8.0f), 1.8f, 1.8f, 0.6f);
    level_add_platform(lvl, vec3(0.0f, 2.4f, -13.0f), 2.0f, 2.0f, 0.6f);

    /* Descend back down toward the goal. */
    level_add_platform(lvl, vec3(0.0f, 1.6f, -19.0f), 2.2f, 2.2f, 0.6f);
    level_add_platform(lvl, vec3(0.0f, 0.8f, -25.0f), 3.0f, 3.0f, 0.6f);

    /* Goal plaza. */
    level_add_platform(lvl, vec3(0.0f, 0.0f, -34.0f), 7.0f, 7.0f, 1.0f);

    /* Collectibles: one easy freebie on the start plaza, then one hovering
       over most of the climbing platforms. */
    level_add_collectible(lvl, vec3(4.0f, 1.0f,  18.0f));
    level_add_collectible(lvl, vec3(0.0f, 1.6f,   2.0f));
    level_add_collectible(lvl, vec3(0.0f, 2.4f,  -3.0f));
    level_add_collectible(lvl, vec3(0.0f, 3.2f,  -8.0f));
    level_add_collectible(lvl, vec3(0.0f, 3.2f, -13.0f));
    level_add_collectible(lvl, vec3(0.0f, 1.6f, -25.0f));

    level_set_start(lvl, vec3(0.0f, 0.0f, 20.0f), 0.0f); /* faces -Z, i.e. into the course */
    level_set_goal(lvl, vec3(0.0f, 1.0f, -34.0f), 3.5f);
}
