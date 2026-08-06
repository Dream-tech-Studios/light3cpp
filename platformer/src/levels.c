/*
 * levels.c - see levels.h. The only file that needs editing to register a
 * new level: one #include and one line in g_levels[].
 */
#include "levels.h"
#include "levels/level01_first_steps.h"
#include "levels/level02_sky_steps.h"
/* @editor-includes -- tools/level_editor.c inserts new #includes ABOVE this line; do not remove */

const LevelDef g_levels[] = {
    { level01_first_steps_build },
    { level02_sky_steps_build },
    /* @editor-registry -- tools/level_editor.c inserts new entries ABOVE this line; do not remove */
};

const int LEVEL_COUNT = (int)(sizeof(g_levels) / sizeof(g_levels[0]));
