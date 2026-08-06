/*
 * levels.h - the level registry: every course in the game, in play order.
 *
 * To add a new level:
 *   1. Create src/levels/levelNN_your_name.h + .c, modeled on
 *      src/levels/level01_first_steps.h/.c - it just needs one function,
 *      `void levelNN_your_name_build(Level *lvl)`, built out of the
 *      level_begin/level_add_platform/level_add_collectible/level_set_start/
 *      level_set_goal helpers declared in level.h.
 *   2. #include its header below and add one line to g_levels[] in
 *      levels.c, in the order you want it to play.
 *   3. That's it - both Makefiles pick up any .c file under src/levels/
 *      automatically (see the wildcard rules), and game.c/app.c drive
 *      progression through g_levels[]/LEVEL_COUNT generically, with no
 *      per-level code anywhere else.
 */
#ifndef LEVELS_H
#define LEVELS_H

#include "level.h"

typedef void (*LevelBuildFn)(Level *lvl);

typedef struct {
    LevelBuildFn build;
} LevelDef;

extern const LevelDef g_levels[];
extern const int      LEVEL_COUNT;

#endif /* LEVELS_H */
