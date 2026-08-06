/*
 * objective.c - see objective.h.
 */
#include "objective.h"

#define OBJECTIVE_READY_SECONDS 2.0f

void objective_init(Objective *o, Level *level) {
    o->level       = level;
    o->state       = OBJECTIVE_READY;
    o->state_timer = OBJECTIVE_READY_SECONDS;
    o->play_time   = 0.0f;
    o->collected   = 0;
}

void objective_update(Objective *o, Vec3 player_pos, float dt) {
    switch (o->state) {
    case OBJECTIVE_READY:
        o->state_timer -= dt;
        if (o->state_timer <= 0.0f) {
            o->state     = OBJECTIVE_PLAYING;
            o->play_time = 0.0f;
        }
        break;

    case OBJECTIVE_PLAYING: {
        float dist_to_goal;
        o->play_time += dt;
        o->collected += level_collect_near(o->level, player_pos, OBJECTIVE_COLLECT_RADIUS);

        dist_to_goal = vec3_len(vec3_sub(player_pos, o->level->goal_pos));
        if (dist_to_goal <= o->level->goal_radius) {
            o->state       = OBJECTIVE_COMPLETE;
            o->state_timer = 0.0f;
        }
        break;
    }

    case OBJECTIVE_COMPLETE:
        o->state_timer += dt;
        break;
    }
}
