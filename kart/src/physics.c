/*
 * physics.c - arcade kart driving model.
 *
 * Frame outline:
 *   1. Split velocity into forward + lateral components (relative to heading).
 *   2. Throttle / brake adjust the forward component.
 *   3. Steering rotates the heading; turn authority scales with speed so the
 *      kart can't spin in place, and reverses sign when reversing.
 *   4. Lateral component is bled out by grip (less grip while drifting -> slide).
 *   5. Recompose world velocity, integrate position.
 *   6. Apply gravity when airborne; snap to the ground mesh otherwise.
 */
#include "physics.h"

void kart_init(Kart *k, Vec3 start_pos, float start_heading) {
    k->max_speed   = 28.0f;
    k->max_reverse = 8.0f;
    k->accel       = 22.0f;
    k->brake_decel = 36.0f;
    /* Coasting from top speed to a dead stop takes max_speed/coast_drag
       seconds - sized so releasing every input brings the kart to rest in
       about a second instead of gliding for several seconds/many units. */
    k->coast_drag  = 30.0f;
    k->turn_rate   = 2.6f;   /* rad/s at full steering authority */
    /* Non-drift grip is meant to feel planted: only 5% of any lateral
       (sideways) velocity survives each second, so drift-free cornering
       slop dies out almost immediately once you stop steering instead of
       carrying the kart sideways across the track. */
    k->grip        = 0.05f;
    k->drift_grip  = 0.985f; /* keeps more lateral vel -> slides wide */
    k->gravity     = 60.0f;

    k->pos       = start_pos;
    k->vel       = vec3_zero();
    k->heading   = start_heading;
    k->on_ground = 1;
}

Vec3 kart_forward(const Kart *k) {
    /* heading 0 faces -Z; +heading rotates toward +X */
    return vec3(sinf(k->heading), 0.0f, -cosf(k->heading));
}

static Vec3 kart_right(const Kart *k) {
    return vec3(cosf(k->heading), 0.0f, sinf(k->heading));
}

float kart_forward_speed(const Kart *k) {
    return vec3_dot(k->vel, kart_forward(k));
}

void kart_update(Kart *k, const KartInput *in, const CollisionMesh *ground, float dt) {
    Vec3 fwd   = kart_forward(k);
    Vec3 right = kart_right(k);

    /* Decompose horizontal velocity into forward/lateral scalars. */
    float v_fwd = vec3_dot(k->vel, fwd);
    float v_lat = vec3_dot(k->vel, right);

    /* ---- longitudinal: throttle / brake / coast ---- */
    if (in->throttle > 0.0f) {
        v_fwd += k->accel * in->throttle * dt;
    }
    if (in->brake > 0.0f) {
        if (v_fwd > 0.0f) {
            /* braking while moving forward */
            v_fwd -= k->brake_decel * in->brake * dt;
            if (v_fwd < 0.0f) v_fwd = 0.0f;
        } else {
            /* already stopped or reversing -> accelerate backward */
            v_fwd -= k->accel * in->brake * dt;
        }
    }
    if (in->throttle == 0.0f && in->brake == 0.0f) {
        /* coast drag pulls speed toward zero */
        if (v_fwd > 0.0f) {
            v_fwd -= k->coast_drag * dt;
            if (v_fwd < 0.0f) v_fwd = 0.0f;
        } else if (v_fwd < 0.0f) {
            v_fwd += k->coast_drag * dt;
            if (v_fwd > 0.0f) v_fwd = 0.0f;
        }
    }
    v_fwd = m3_clampf(v_fwd, -k->max_reverse, k->max_speed);

    /* ---- steering ---- */
    /* Authority ramps in quickly with speed, then saturates, so the kart
       turns well at cruising speed but can't pivot from a standstill. */
    {
        float speed_ratio = v_fwd / k->max_speed;
        if (speed_ratio < 0.0f) speed_ratio = -speed_ratio; /* allow turning in reverse */
        if (speed_ratio > 1.0f) speed_ratio = 1.0f;
        {
            float authority = speed_ratio * (2.0f - speed_ratio); /* ease-in curve */
            float dir = (v_fwd >= 0.0f) ? 1.0f : -1.0f;           /* invert steer in reverse */
            k->heading += in->steer * k->turn_rate * authority * dir * dt;
        }
    }

    /* ---- lateral grip / drift ---- */
    {
        float grip = in->drift ? k->drift_grip : k->grip;
        /* Convert per-second retention into per-step factor. */
        float keep = powf(grip, dt);
        v_lat *= keep;
    }

    /* ---- recompose world velocity (horizontal) ---- */
    fwd   = kart_forward(k);   /* heading changed above */
    right = kart_right(k);
    {
        Vec3 horiz = vec3_add(vec3_scale(fwd, v_fwd), vec3_scale(right, v_lat));
        k->vel.x = horiz.x;
        k->vel.z = horiz.z;
    }

    /* ---- gravity ---- */
    if (!k->on_ground) {
        k->vel.y -= k->gravity * dt;
    }

    /* ---- integrate ---- */
    k->pos = vec3_add(k->pos, vec3_scale(k->vel, dt));

    /* ---- ground snap ---- */
    {
        float ground_y;
        Vec3  n;
        if (track_ground(ground, k->pos, &ground_y, &n)) {
            if (k->pos.y <= ground_y + 0.001f) {
                k->pos.y = ground_y;
                if (k->vel.y < 0.0f) k->vel.y = 0.0f;
                k->on_ground = 1;
            } else {
                k->on_ground = 0;
            }
        } else {
            k->on_ground = 0;
        }
    }
}
