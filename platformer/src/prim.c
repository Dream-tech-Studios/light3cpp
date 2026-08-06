/*
 * prim.c - see prim.h. Pure geometry + malloc, so it builds on both
 * PLATFORM_PC and PLATFORM_GC (same guard as obj_loader.c).
 */
#if defined(PLATFORM_PC) || defined(PLATFORM_GC)

#include <stdlib.h>
#include "prim.h"

#define PRIM_SPHERE_LAT 12
#define PRIM_SPHERE_LON 18

static void set_material(Mesh *m, float r, float g, float b) {
    m->material_count = 1;
    m->materials[0].name[0] = '\0';
    m->materials[0].r = r;
    m->materials[0].g = g;
    m->materials[0].b = b;
}

/* Append one triangle. Caller must have allocated m->verts with room; the
   normal is computed geometrically and flipped to agree with `hint` so
   shading stays correct regardless of the vertex order passed in. */
static void emit_tri(Mesh *m, Vec3 a, Vec3 b, Vec3 c, Vec3 hint) {
    Vec3 n = vec3_norm(vec3_cross(vec3_sub(b, a), vec3_sub(c, a)));
    MeshVertex v;
    if (vec3_dot(n, hint) < 0.0f) n = vec3_scale(n, -1.0f);
    v.normal = n; v.u = 0.0f; v.v = 0.0f; v.mtl = 0;
    v.pos = a; m->verts[m->vert_count++] = v;
    v.pos = b; m->verts[m->vert_count++] = v;
    v.pos = c; m->verts[m->vert_count++] = v;
}

static void compute_bounds(Mesh *m) {
    int i;
    if (m->vert_count == 0) { m->bounds_min = m->bounds_max = vec3_zero(); return; }
    m->bounds_min = m->bounds_max = m->verts[0].pos;
    for (i = 1; i < m->vert_count; ++i) {
        Vec3 p = m->verts[i].pos;
        if (p.x < m->bounds_min.x) m->bounds_min.x = p.x;
        if (p.y < m->bounds_min.y) m->bounds_min.y = p.y;
        if (p.z < m->bounds_min.z) m->bounds_min.z = p.z;
        if (p.x > m->bounds_max.x) m->bounds_max.x = p.x;
        if (p.y > m->bounds_max.y) m->bounds_max.y = p.y;
        if (p.z > m->bounds_max.z) m->bounds_max.z = p.z;
    }
}

int prim_build_slope(Mesh *m, Vec3 h, float r, float g, float b) {
    /* corners: bottom-back A,B  bottom-front C,D  top-front E,F */
    Vec3 A = vec3(-h.x,-h.y,-h.z), B = vec3( h.x,-h.y,-h.z);
    Vec3 C = vec3(-h.x,-h.y, h.z), D = vec3( h.x,-h.y, h.z);
    Vec3 E = vec3(-h.x, h.y, h.z), F = vec3( h.x, h.y, h.z);

    m->vert_count = 0;
    m->verts = (MeshVertex *)malloc(sizeof(MeshVertex) * 8 * 3); /* 8 tris */
    if (!m->verts) return 0;
    set_material(m, r, g, b);

    /* ramp (walk) face - normal points up */
    emit_tri(m, A, B, F, vec3(0,1,0));
    emit_tri(m, A, F, E, vec3(0,1,0));
    /* bottom */
    emit_tri(m, A, C, D, vec3(0,-1,0));
    emit_tri(m, A, D, B, vec3(0,-1,0));
    /* front vertical (z = +h.z) */
    emit_tri(m, C, D, F, vec3(0,0,1));
    emit_tri(m, C, F, E, vec3(0,0,1));
    /* left (x = -h.x) */
    emit_tri(m, A, C, E, vec3(-1,0,0));
    /* right (x = +h.x) */
    emit_tri(m, B, F, D, vec3(1,0,0));

    compute_bounds(m);
    return 1;
}

int prim_build_sphere(Mesh *m, Vec3 rad, float r, float g, float b) {
    int i, j;
    int tris = PRIM_SPHERE_LAT * PRIM_SPHERE_LON * 2;

    m->vert_count = 0;
    m->verts = (MeshVertex *)malloc(sizeof(MeshVertex) * (size_t)tris * 3);
    if (!m->verts) return 0;
    set_material(m, r, g, b);

    for (i = 0; i < PRIM_SPHERE_LAT; ++i) {
        float t0 = M3_PI * ((float)i / PRIM_SPHERE_LAT - 0.5f);
        float t1 = M3_PI * ((float)(i + 1) / PRIM_SPHERE_LAT - 0.5f);
        for (j = 0; j < PRIM_SPHERE_LON; ++j) {
            float p0 = 2.0f * M3_PI * ((float)j / PRIM_SPHERE_LON);
            float p1 = 2.0f * M3_PI * ((float)(j + 1) / PRIM_SPHERE_LON);
            Vec3 d00 = vec3(cosf(t0)*cosf(p0), sinf(t0), cosf(t0)*sinf(p0));
            Vec3 d01 = vec3(cosf(t0)*cosf(p1), sinf(t0), cosf(t0)*sinf(p1));
            Vec3 d11 = vec3(cosf(t1)*cosf(p1), sinf(t1), cosf(t1)*sinf(p1));
            Vec3 d10 = vec3(cosf(t1)*cosf(p0), sinf(t1), cosf(t1)*sinf(p0));
            Vec3 v00 = vec3(d00.x*rad.x, d00.y*rad.y, d00.z*rad.z);
            Vec3 v01 = vec3(d01.x*rad.x, d01.y*rad.y, d01.z*rad.z);
            Vec3 v11 = vec3(d11.x*rad.x, d11.y*rad.y, d11.z*rad.z);
            Vec3 v10 = vec3(d10.x*rad.x, d10.y*rad.y, d10.z*rad.z);
            /* outward hint = the (unit) direction to the quad's midpoint */
            Vec3 hint = vec3_add(vec3_add(d00, d01), vec3_add(d11, d10));
            emit_tri(m, v00, v01, v11, hint);
            emit_tri(m, v00, v11, v10, hint);
        }
    }
    compute_bounds(m);
    return 1;
}

#endif /* PLATFORM_PC || PLATFORM_GC */
