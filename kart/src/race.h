/*
 * race.h - race rules and state machine layered on top of a Circuit.
 *
 * Knows nothing about rendering or input; game.c drives it with just the
 * kart's world position each frame, and reads back the public fields to
 * feed the HUD.
 */
#ifndef RACE_H
#define RACE_H

#include "math3d.h"
#include "circuit.h"

typedef enum {
    RACE_COUNTDOWN,
    RACE_RUNNING,
    RACE_FINISHED
} RaceState;

/* Seconds of "GO!" text shown right after the countdown ends, while
   already RACE_RUNNING (kept here so hud.c can share the same constant). */
#define RACE_GO_FLASH_SECONDS 0.75f

typedef struct {
    const Circuit *circuit;      /* borrowed, must outlive the Race */
    RaceState      state;
    float          state_timer;  /* countdown remaining (COUNTDOWN) or time since finish (FINISHED) */
    float          race_time;    /* elapsed time while RUNNING; freezes at finish */
    int            total_laps;
    int            current_lap;  /* 1-based, clamped to total_laps once finished */
    int            next_checkpoint;
    Vec3           prev_pos;
} Race;

void race_init(Race *r, const Circuit *circuit, int total_laps, Vec3 start_pos);

/* Advances the state machine by dt using the kart's current world position.
   `restart_requested` re-starts the race (back to a fresh countdown) but
   only takes effect while RACE_FINISHED. */
void race_update(Race *r, Vec3 kart_pos, float dt, int restart_requested);

/* Racer's current placement (1 = first). Always 1 today (no AI racers);
   once other racers exist this becomes a rank among all of them. */
int race_position(const Race *r);

#endif /* RACE_H */
