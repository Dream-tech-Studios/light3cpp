/*
 * objective.h - level-completion state machine layered on top of a Level.
 *
 * Knows nothing about rendering or input; game.c drives it with just the
 * player's world position each frame, and reads back the public fields to
 * feed the HUD.
 */
#ifndef OBJECTIVE_H
#define OBJECTIVE_H

#include "math3d.h"
#include "level.h"

typedef enum {
    OBJECTIVE_READY,     /* brief pause before the timer starts */
    OBJECTIVE_PLAYING,
    OBJECTIVE_COMPLETE
} ObjectiveState;

/* Seconds of "GO!" text shown right after OBJECTIVE_READY ends, while
   already OBJECTIVE_PLAYING (kept here so hud.c can share the same constant). */
#define OBJECTIVE_GO_FLASH_SECONDS 0.75f

/* How close (world units) the player needs to be to a collectible/the goal
   to pick it up / finish the level. */
#define OBJECTIVE_COLLECT_RADIUS 1.2f

typedef struct {
    Level         *level;        /* borrowed, must outlive the Objective; its
                                     collectibles get mutated as they're picked up */
    ObjectiveState state;
    float          state_timer;  /* countdown remaining (READY) or time since
                                     completion (COMPLETE) */
    float          play_time;    /* elapsed time while PLAYING; freezes at completion */
    int            collected;    /* collectibles picked up so far */
} Objective;

void objective_init(Objective *o, Level *level);

/* Advances the state machine by dt using the player's current world
   position: collects nearby items while PLAYING and checks the goal. */
void objective_update(Objective *o, Vec3 player_pos, float dt);

#endif /* OBJECTIVE_H */
