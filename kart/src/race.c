/*
 * race.c - see race.h.
 */
#include <math.h>
#include "race.h"

#define COUNTDOWN_SECONDS 3.0f

void race_init(Race *r, const Circuit *circuit, int total_laps, Vec3 start_pos) {
    r->circuit         = circuit;
    r->state           = RACE_COUNTDOWN;
    r->state_timer     = COUNTDOWN_SECONDS;
    r->race_time       = 0.0f;
    r->total_laps      = total_laps;
    r->current_lap     = 1;
    /* Checkpoint 0 sits under the kart at the start line, so the first
       gate the kart needs to reach is checkpoint 1. */
    r->next_checkpoint = (circuit->checkpoint_count > 1) ? 1 : 0;
    r->prev_pos        = start_pos;
}

static int crossed_checkpoint(const Checkpoint *cp, Vec3 prev_pos, Vec3 now_pos) {
    Vec3  across_axis = vec3(-cp->forward.z, 0.0f, cp->forward.x);
    float along_prev  = vec3_dot(vec3_sub(prev_pos, cp->center), cp->forward);
    float along_now   = vec3_dot(vec3_sub(now_pos, cp->center), cp->forward);
    float across_now  = vec3_dot(vec3_sub(now_pos, cp->center), across_axis);

    return along_prev < 0.0f && along_now >= 0.0f && fabsf(across_now) <= cp->half_width;
}

void race_update(Race *r, Vec3 kart_pos, float dt, int restart_requested) {
    switch (r->state) {
    case RACE_COUNTDOWN:
        r->state_timer -= dt;
        r->prev_pos = kart_pos;
        if (r->state_timer <= 0.0f) {
            r->state     = RACE_RUNNING;
            r->race_time = 0.0f;
        }
        break;

    case RACE_RUNNING: {
        const Checkpoint *cp = &r->circuit->checkpoints[r->next_checkpoint];
        r->race_time += dt;

        if (crossed_checkpoint(cp, r->prev_pos, kart_pos)) {
            r->next_checkpoint++;
            if (r->next_checkpoint >= r->circuit->checkpoint_count) {
                r->next_checkpoint = 0;
                r->current_lap++;
                if (r->current_lap > r->total_laps) {
                    r->current_lap = r->total_laps;
                    r->state       = RACE_FINISHED;
                    r->state_timer = 0.0f;
                }
            }
        }
        r->prev_pos = kart_pos;
        break;
    }

    case RACE_FINISHED:
        r->state_timer += dt;
        if (restart_requested) {
            const Circuit *circuit    = r->circuit;
            int            total_laps = r->total_laps;
            race_init(r, circuit, total_laps, circuit->start_pos);
        }
        break;
    }
}

int race_position(const Race *r) {
    (void)r;
    return 1;
}
