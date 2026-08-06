/*
 * track.c - collision mesh construction and ray queries.
 *
 * Uses the Moller-Trumbore ray/triangle intersection. No spatial
 * acceleration yet (linear scan); fine for small meshes like a flat
 * plane or a simple test track. A grid/BVH can slot in behind the same
 * track_raycast() signature later without touching the physics code.
 */
#include <stdlib.h>
#include "track.h"

static Triangle make_tri(Vec3 a, Vec3 b, Vec3 c) {
    Triangle t;
    t.a = a; t.b = b; t.c = c;
    t.normal = vec3_norm(vec3_cross(vec3_sub(b, a), vec3_sub(c, a)));
    return t;
}

void track_make_plane(CollisionMesh *cm, float half_size, float y) {
    Vec3 p0 = vec3(-half_size, y, -half_size);
    Vec3 p1 = vec3( half_size, y, -half_size);
    Vec3 p2 = vec3( half_size, y,  half_size);
    Vec3 p3 = vec3(-half_size, y,  half_size);

    cm->tris = (Triangle *)malloc(sizeof(Triangle) * 2);
    cm->count = 2;
    /* CCW when viewed from above so the normal points +Y */
    cm->tris[0] = make_tri(p0, p2, p1);
    cm->tris[1] = make_tri(p0, p3, p2);
}

void track_add_platform_top(CollisionMesh *cm, Vec3 center, float half_x, float half_z) {
    float y  = center.y;
    Vec3 p0 = vec3(center.x - half_x, y, center.z - half_z);
    Vec3 p1 = vec3(center.x + half_x, y, center.z - half_z);
    Vec3 p2 = vec3(center.x + half_x, y, center.z + half_z);
    Vec3 p3 = vec3(center.x - half_x, y, center.z + half_z);

    cm->tris = (Triangle *)realloc(cm->tris, sizeof(Triangle) * (size_t)(cm->count + 2));
    /* CCW when viewed from above so the normal points +Y, same as track_make_plane. */
    cm->tris[cm->count++] = make_tri(p0, p2, p1);
    cm->tris[cm->count++] = make_tri(p0, p3, p2);
}

void track_add_mesh(CollisionMesh *cm, const Mesh *mesh, Vec3 pos, float yaw, float scale) {
    float cs = cosf(yaw), sn = sinf(yaw);
    int tris = mesh->vert_count / 3;
    int i;
    if (tris <= 0) return;

    cm->tris = (Triangle *)realloc(cm->tris, sizeof(Triangle) * (size_t)(cm->count + tris));
    for (i = 0; i < tris; ++i) {
        Vec3 out[3];
        int k;
        for (k = 0; k < 3; ++k) {
            /* scale, then rotate about Y (matching mat4_rotate_y), then translate */
            Vec3 p = vec3_scale(mesh->verts[i * 3 + k].pos, scale);
            Vec3 rp = vec3(p.x * cs + p.z * sn, p.y, -p.x * sn + p.z * cs);
            out[k] = vec3_add(rp, pos);
        }
        cm->tris[cm->count++] = make_tri(out[0], out[1], out[2]);
    }
}

void track_free(CollisionMesh *cm) {
    free(cm->tris);
    cm->tris = NULL;
    cm->count = 0;
}

static int ray_tri(Vec3 orig, Vec3 dir, const Triangle *tri, float *out_t) {
    const float EPS = 1e-7f;
    Vec3 e1 = vec3_sub(tri->b, tri->a);
    Vec3 e2 = vec3_sub(tri->c, tri->a);
    Vec3 p  = vec3_cross(dir, e2);
    float det = vec3_dot(e1, p);
    Vec3 tvec, q;
    float inv, u, v, t;

    if (det > -EPS && det < EPS) return 0; /* parallel */
    inv = 1.0f / det;

    tvec = vec3_sub(orig, tri->a);
    u = vec3_dot(tvec, p) * inv;
    if (u < 0.0f || u > 1.0f) return 0;

    q = vec3_cross(tvec, e1);
    v = vec3_dot(dir, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return 0;

    t = vec3_dot(e2, q) * inv;
    if (t < 0.0f) return 0;

    *out_t = t;
    return 1;
}

int track_raycast(const CollisionMesh *cm, Vec3 origin, Vec3 dir,
                  float *out_t, Vec3 *out_normal) {
    int i, hit = 0;
    float best = 1e30f, t;

    for (i = 0; i < cm->count; ++i) {
        if (ray_tri(origin, dir, &cm->tris[i], &t) && t < best) {
            best = t;
            hit = 1;
            if (out_normal) *out_normal = cm->tris[i].normal;
        }
    }
    if (hit && out_t) *out_t = best;
    return hit;
}

int track_ground(const CollisionMesh *cm, Vec3 pos,
                 float *out_y, Vec3 *out_normal) {
    /* Start well above the player and cast down so we catch the surface
       even if the player has sunk slightly below it. */
    Vec3 origin = vec3(pos.x, pos.y + 100.0f, pos.z);
    Vec3 down   = vec3(0.0f, -1.0f, 0.0f);
    float t;
    if (track_raycast(cm, origin, down, &t, out_normal)) {
        if (out_y) *out_y = origin.y - t; /* dir is unit -Y */
        return 1;
    }
    return 0;
}
