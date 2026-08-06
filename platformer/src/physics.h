/*
 * physics.h - arcade 3D-platformer movement model.
 *
 * Design goals:
 *   - Arcade feel (snappy, forgiving), not a sim.
 *   - Every behavior driven by a public, named tuning field so the feel
 *     can be changed without touching the integration code.
 *   - Walks over an arbitrary CollisionMesh ground (flat plane + any
 *     platform tops baked into the same triangle soup - see level.h).
 */
#ifndef PHYSICS_H
#define PHYSICS_H

#include "math3d.h"
#include "input.h"
#include "track.h"

typedef struct {
    /* ---- tuning (edit freely) ---- */
    float max_speed;      /* top forward speed, units/s */
    float max_back_speed; /* top backward speed, units/s (slower than forward) */
    float accel;          /* acceleration toward the target speed, units/s^2 */
    float decel;          /* deceleration toward zero/target when input eases off */
    float turn_rate;      /* turning yaw rate, rad/s (always available, even standing still) */
    float jump_speed;     /* upward velocity applied on a jump, units/s */
    float gravity;        /* downward accel when airborne, units/s^2 */
    float max_step_up;    /* tallest ground-height increase walkable without jumping;
                              taller ledges block horizontal movement like a wall until
                              you jump up onto them (see player_update). */

    /* ---- runtime state ---- */
    Vec3  pos;            /* world position (feet, at ground contact) */
    Vec3  vel;            /* world-space velocity */
    float heading;        /* yaw, radians (0 = facing -Z) */
    int   on_ground;
    int   prev_jump;      /* last frame's jump button state, for edge detection */
} Player;

/* Initialize with sensible arcade defaults at the given start position and
   heading (radians, matching player_forward's convention: 0 = facing -Z). */
void player_init(Player *p, Vec3 start_pos, float start_heading);

/* Advance one step. `dt` should be a fixed timestep (e.g. 1/60). */
void player_update(Player *p, const PlayerInput *in, const CollisionMesh *ground, float dt);

/* Forward unit vector implied by the player's heading. */
Vec3 player_forward(const Player *p);

#endif /* PHYSICS_H */
