/*
 * physics.c - arcade 3D-platformer movement model.
 *
 * Frame outline:
 *   1. Turning is always available (tank-style: turn left/right, walk
 *      forward/back along the resulting facing), independent of speed.
 *   2. Forward speed eases toward the input's target speed.
 *   3. A jump impulse is applied on a fresh press of the jump button, only
 *      while grounded.
 *   4. Gravity pulls the player down while airborne.
 *   5. Integrate position, then query the ground mesh straight below the
 *      new position: small height increases (steps/ramps) are walked up
 *      for free, but a ledge taller than max_step_up blocks horizontal
 *      movement like a wall - the only way onto something that tall is to
 *      jump, since the step-height check is skipped entirely while
 *      airborne (see `was_grounded` below).
 */
#include "physics.h"

void player_init(Player *p, Vec3 start_pos, float start_heading) {
    p->max_speed      = 9.0f;
    p->max_back_speed = 4.5f;
    p->accel          = 40.0f;
    p->decel          = 50.0f;
    p->turn_rate      = 3.2f;   /* rad/s */
    p->jump_speed     = 14.0f;
    p->gravity        = 42.0f;
    p->max_step_up    = 0.4f;

    p->pos       = start_pos;
    p->vel       = vec3_zero();
    p->heading   = start_heading;
    p->on_ground = 1;
    p->prev_jump = 0;
}

Vec3 player_forward(const Player *p) {
    /* heading 0 faces -Z; +heading rotates toward +X */
    return vec3(sinf(p->heading), 0.0f, -cosf(p->heading));
}

void player_update(Player *p, const PlayerInput *in, const CollisionMesh *ground, float dt) {
    Vec3  fwd;
    float v_fwd;
    Vec3  old_pos;
    int   was_grounded = p->on_ground;
    int   jump_pressed = in->jump && !p->prev_jump;
    p->prev_jump = in->jump;

    /* ---- turning ---- */
    p->heading += in->turn * p->turn_rate * dt;
    fwd = player_forward(p);

    /* ---- forward/back speed: ease toward the requested target ---- */
    v_fwd = vec3_dot(p->vel, fwd);
    {
        float target = in->move * ((in->move >= 0.0f) ? p->max_speed : p->max_back_speed);
        float rate   = (fabsf(target) > fabsf(v_fwd)) ? p->accel : p->decel;
        if (v_fwd < target) {
            v_fwd += rate * dt;
            if (v_fwd > target) v_fwd = target;
        } else if (v_fwd > target) {
            v_fwd -= rate * dt;
            if (v_fwd < target) v_fwd = target;
        }
    }
    p->vel.x = fwd.x * v_fwd;
    p->vel.z = fwd.z * v_fwd;

    /* ---- jump ---- */
    if (jump_pressed && p->on_ground) {
        p->vel.y = p->jump_speed;
        p->on_ground = 0;
    }

    /* ---- gravity ---- */
    if (!p->on_ground) {
        p->vel.y -= p->gravity * dt;
    }

    /* ---- integrate ---- */
    old_pos = p->pos;
    p->pos  = vec3_add(p->pos, vec3_scale(p->vel, dt));

    /* ---- ground / step collision ---- */
    {
        float ground_y;
        Vec3  n;
        if (track_ground(ground, p->pos, &ground_y, &n)) {
            if (was_grounded && (ground_y - old_pos.y) > p->max_step_up) {
                /* Ledge too tall to walk up - block the horizontal move
                   like a wall and re-query the ground under the reverted
                   spot (still the old, walkable surface). */
                p->pos.x = old_pos.x;
                p->pos.z = old_pos.z;
                p->vel.x = 0.0f;
                p->vel.z = 0.0f;
                track_ground(ground, p->pos, &ground_y, &n);
            }
            if (p->pos.y <= ground_y + 0.001f) {
                p->pos.y = ground_y;
                if (p->vel.y < 0.0f) p->vel.y = 0.0f;
                p->on_ground = 1;
            } else {
                p->on_ground = 0;
            }
        } else {
            p->on_ground = 0;
        }
    }
}
