/*
 * circuit.c - see circuit.h.
 *
 * Layout (viewed from above, +X right, +Z toward the camera at heading 0):
 *
 *        D-------------------C
 *        |                   |
 *        A-------------------B    <- checkpoints[0] / start, facing +X
 *
 * A/B/C/D is the centerline rectangle; walls are the same rectangle offset
 * inward and outward by half the track width.
 */
#include <stdlib.h>
#include <math.h>
#include "circuit.h"

#define WALL_THICKNESS 0.6f /* kart is kept at least radius + this/2 away from a wall */

static Vec3 closest_point_on_segment_xz(Vec3 a, Vec3 b, Vec3 p) {
    Vec3 ab = vec3_sub(b, a);
    Vec3 ap = vec3_sub(p, a);
    float ab_len2 = ab.x * ab.x + ab.z * ab.z;
    float t = (ab_len2 > 1e-9f) ? (ap.x * ab.x + ap.z * ab.z) / ab_len2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return vec3(a.x + ab.x * t, p.y, a.z + ab.z * t);
}

/* Inverse of physics.c's kart_forward(heading) = (sin(h),0,-cos(h)),
   duplicated here (rather than depending on physics.h) so circuit.c stays
   decoupled from the Kart type, exactly like track.c is. */
static float heading_from_forward(Vec3 f) {
    return atan2f(f.x, -f.z);
}

void circuit_make_oval(Circuit *c, float length, float depth, float track_width) {
    float hl = length * 0.5f;
    float hd = depth * 0.5f;
    float hw = track_width * 0.5f;

    Vec3 a  = vec3(-hl, 0.0f,  hd);
    Vec3 b  = vec3( hl, 0.0f,  hd);
    Vec3 cn = vec3( hl, 0.0f, -hd);
    Vec3 d  = vec3(-hl, 0.0f, -hd);

    Vec3 oa = vec3(-(hl + hw), 0.0f,  (hd + hw));
    Vec3 ob = vec3( (hl + hw), 0.0f,  (hd + hw));
    Vec3 oc = vec3( (hl + hw), 0.0f, -(hd + hw));
    Vec3 od = vec3(-(hl + hw), 0.0f, -(hd + hw));

    Vec3 ia = vec3(-(hl - hw), 0.0f,  (hd - hw));
    Vec3 ib = vec3( (hl - hw), 0.0f,  (hd - hw));
    Vec3 ic = vec3( (hl - hw), 0.0f, -(hd - hw));
    Vec3 id = vec3(-(hl - hw), 0.0f, -(hd - hw));

    c->wall_count = 8;
    c->walls = (Wall *)malloc(sizeof(Wall) * (size_t)c->wall_count);
    c->walls[0].a = oa; c->walls[0].b = ob;
    c->walls[1].a = ob; c->walls[1].b = oc;
    c->walls[2].a = oc; c->walls[2].b = od;
    c->walls[3].a = od; c->walls[3].b = oa;
    c->walls[4].a = ia; c->walls[4].b = ib;
    c->walls[5].a = ib; c->walls[5].b = ic;
    c->walls[6].a = ic; c->walls[6].b = id;
    c->walls[7].a = id; c->walls[7].b = ia;

    c->checkpoint_count = 4;
    c->checkpoints = (Checkpoint *)malloc(sizeof(Checkpoint) * (size_t)c->checkpoint_count);

    c->checkpoints[0].center     = vec3_scale(vec3_add(a, b), 0.5f);
    c->checkpoints[0].forward    = vec3_norm(vec3_sub(b, a));
    c->checkpoints[0].half_width = hw;

    c->checkpoints[1].center     = vec3_scale(vec3_add(b, cn), 0.5f);
    c->checkpoints[1].forward    = vec3_norm(vec3_sub(cn, b));
    c->checkpoints[1].half_width = hw;

    c->checkpoints[2].center     = vec3_scale(vec3_add(cn, d), 0.5f);
    c->checkpoints[2].forward    = vec3_norm(vec3_sub(d, cn));
    c->checkpoints[2].half_width = hw;

    c->checkpoints[3].center     = vec3_scale(vec3_add(d, a), 0.5f);
    c->checkpoints[3].forward    = vec3_norm(vec3_sub(a, d));
    c->checkpoints[3].half_width = hw;

    c->start_pos     = c->checkpoints[0].center;
    c->start_heading = heading_from_forward(c->checkpoints[0].forward);
}

void circuit_free(Circuit *c) {
    free(c->walls);
    free(c->checkpoints);
    c->walls = NULL;
    c->checkpoints = NULL;
    c->wall_count = 0;
    c->checkpoint_count = 0;
}

void circuit_resolve_walls(const Circuit *c, Vec3 *pos, Vec3 *vel, float radius) {
    int i;
    float keep_out = radius + WALL_THICKNESS * 0.5f;

    for (i = 0; i < c->wall_count; ++i) {
        Vec3 cp = closest_point_on_segment_xz(c->walls[i].a, c->walls[i].b, *pos);
        Vec3 diff = vec3_sub(*pos, cp);
        float dist;
        diff.y = 0.0f;
        dist = vec3_len(diff);

        if (dist < keep_out && dist > 1e-6f) {
            Vec3 n = vec3_scale(diff, 1.0f / dist);
            float push = keep_out - dist;
            float into = vel->x * n.x + vel->z * n.z;

            pos->x += n.x * push;
            pos->z += n.z * push;

            if (into < 0.0f) {
                vel->x -= n.x * into;
                vel->z -= n.z * into;
            }
        }
    }
}
