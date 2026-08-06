/*
 * physics.h - arcade kart driving model.
 *
 * Design goals:
 *   - Arcade feel (snappy, forgiving), not a sim.
 *   - Every behavior driven by a public, named tuning field so the feel
 *     can be changed without touching the integration code.
 *   - Drives over an arbitrary CollisionMesh ground (flat plane by default).
 */
#ifndef PHYSICS_H
#define PHYSICS_H

#include "math3d.h"
#include "input.h"
#include "track.h"

typedef struct {
    /* ---- tuning (edit freely) ---- */
    float max_speed;      /* top forward speed, units/s */
    float max_reverse;    /* top reverse speed, units/s */
    float accel;          /* throttle acceleration, units/s^2 */
    float brake_decel;    /* brake/reverse deceleration, units/s^2 */
    float coast_drag;     /* passive slow-down when no input, units/s^2 */
    float turn_rate;      /* max steering yaw rate, rad/s */
    float grip;           /* lateral velocity kept per second (0..1) when gripping */
    float drift_grip;     /* lateral grip while drifting (lower = more slide) */
    float gravity;        /* downward accel when airborne, units/s^2 */

    /* ---- runtime state ---- */
    Vec3  pos;            /* world position (kart origin at ground contact) */
    Vec3  vel;            /* world-space velocity */
    float heading;        /* yaw, radians (0 = facing -Z) */
    int   on_ground;
} Kart;

/* Initialize with sensible arcade defaults at the given start position and
   heading (radians, matching kart_forward's convention: 0 = facing -Z). */
void kart_init(Kart *k, Vec3 start_pos, float start_heading);

/* Advance one step. `dt` should be a fixed timestep (e.g. 1/60). */
void kart_update(Kart *k, const KartInput *in, const CollisionMesh *ground, float dt);

/* Forward unit vector implied by the kart heading. */
Vec3 kart_forward(const Kart *k);

/* Signed speed along the forward axis (negative = reversing). */
float kart_forward_speed(const Kart *k);

#endif /* PHYSICS_H */
