/*
 * obj_loader.c - see obj_loader.h.
 *
 * Single linear pass over the file: collect v / vn / vt into growable
 * scratch arrays, then expand each face into de-indexed triangle vertices
 * tagged with the current material. Material colors are assigned by name
 * as a fallback tint for meshes with no bound texture (see vehicle.h);
 * when a texture is bound, render_draw_mesh samples it using each
 * vertex's (u, v) instead.
 *
 * Runtime file loading needs a filesystem and a heap: on desktop that's
 * just the OS filesystem, on GameCube it's the "gcdvd:" device gcdvd.c
 * registers over the disc's own FST (see vehicle.c/ASSET_ROOT), so this
 * file builds for both PLATFORM_PC and PLATFORM_GC. tools/obj2c.c also
 * compiles this same source (with PLATFORM_PC defined) to bake a mesh to
 * a C header offline, for anyone who still wants a no-disc-access
 * fallback vehicle, so the parser is written exactly once and shared by
 * all three.
 */
#if defined(PLATFORM_PC) || defined(PLATFORM_GC)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "obj_loader.h"

/* ---- growable float/vec scratch ---- */
typedef struct { Vec3 *data; int count, cap; } VecArr;

static void vecarr_push(VecArr *a, Vec3 v) {
    if (a->count == a->cap) {
        a->cap = a->cap ? a->cap * 2 : 256;
        a->data = (Vec3 *)realloc(a->data, sizeof(Vec3) * a->cap);
    }
    a->data[a->count++] = v;
}

/* Texture coordinates only need 2 components; reuse Vec3 with z unused so
   the growable-array plumbing above stays shared with positions/normals. */
typedef VecArr UvArr;
static void uvarr_push(UvArr *a, float u, float v) { vecarr_push(a, vec3(u, v, 0.0f)); }

static void verts_push(Mesh *m, MeshVertex v, int *cap) {
    if (m->vert_count == *cap) {
        *cap = *cap ? *cap * 2 : 512;
        m->verts = (MeshVertex *)realloc(m->verts, sizeof(MeshVertex) * (*cap));
    }
    m->verts[m->vert_count++] = v;
}

/* Pick a color for a material from its name (no texture support yet). */
static void material_color(const char *name, float *r, float *g, float *b) {
    if (strstr(name, "rim"))         { *r = 0.78f; *g = 0.78f; *b = 0.82f; } /* check before "tire" */
    else if (strstr(name, "tire"))   { *r = 0.07f; *g = 0.07f; *b = 0.08f; }
    else if (strstr(name, "screen")) { *r = 0.20f; *g = 0.72f; *b = 0.90f; }
    else if (strstr(name, "body"))   { *r = 0.85f; *g = 0.16f; *b = 0.16f; }
    else                             { *r = 0.80f; *g = 0.80f; *b = 0.80f; }
}

static int find_or_add_material(Mesh *m, const char *name) {
    int i;
    for (i = 0; i < m->material_count; ++i)
        if (strcmp(m->materials[i].name, name) == 0) return i;

    if (m->material_count >= MESH_MAX_MATERIALS)
        return m->material_count ? m->material_count - 1 : 0;

    {
        MeshMaterial *mt = &m->materials[m->material_count];
        strncpy(mt->name, name, sizeof(mt->name) - 1);
        mt->name[sizeof(mt->name) - 1] = '\0';
        material_color(mt->name, &mt->r, &mt->g, &mt->b);
    }
    return m->material_count++;
}

/* Parse one face-vertex token "v", "v/vt", "v//vn" or "v/vt/vn".
   Returns 0-based indices; *vi is required, *ti and *ni may be -1 if absent. */
static void parse_face_vert(const char *tok, int vcount, int tcount, int ncount,
                            int *vi, int *ti, int *ni) {
    int v = 0, t = 0, n = 0;
    const char *p = tok;

    *vi = 0; *ti = -1; *ni = -1;

    v = atoi(p);
    *vi = (v < 0) ? vcount + v : v - 1;

    /* skip to first '/' */
    while (*p && *p != '/') ++p;
    if (*p != '/') return;          /* "v" only */
    ++p;
    if (*p && *p != '/') {
        t = atoi(p);
        *ti = (t < 0) ? tcount + t : t - 1;
    }
    while (*p && *p != '/') ++p;
    if (*p != '/') return;          /* "v/vt" only */
    ++p;
    if (*p && *p != ' ') {
        n = atoi(p);
        *ni = (n < 0) ? ncount + n : n - 1;
    }
}

int mesh_load_obj(Mesh *m, const char *path) {
    FILE *f = fopen(path, "r");
    char line[512];
    VecArr positions = {0,0,0};
    VecArr normals   = {0,0,0};
    UvArr  uvs       = {0,0,0};
    int cur_mtl = 0;
    int verts_cap = 0;
    int have_any_mtl = 0;

    if (!f) return 0;

    memset(m, 0, sizeof(*m));

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            Vec3 v;
            if (sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z) == 3)
                vecarr_push(&positions, v);
        } else if (line[0] == 'v' && line[1] == 'n' && line[2] == ' ') {
            Vec3 v;
            if (sscanf(line + 3, "%f %f %f", &v.x, &v.y, &v.z) == 3)
                vecarr_push(&normals, v);
        } else if (line[0] == 'v' && line[1] == 't' && line[2] == ' ') {
            float u, v;
            if (sscanf(line + 3, "%f %f", &u, &v) == 2)
                uvarr_push(&uvs, u, 1.0f - v); /* flip: OBJ's v=0 is the bottom row */
        } else if (strncmp(line, "usemtl", 6) == 0) {
            char name[64];
            if (sscanf(line + 6, "%63s", name) == 1) {
                cur_mtl = find_or_add_material(m, name);
                have_any_mtl = 1;
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            /* gather polygon vertices, then fan-triangulate */
            int   poly_v[32];
            int   poly_t[32];
            int   poly_n[32];
            int   poly_count = 0;
            char *tok;
            char *s = line + 2;

            if (!have_any_mtl) { cur_mtl = find_or_add_material(m, "default"); have_any_mtl = 1; }

            /* Plain strtok (not strtok_r): this parser never tokenizes more
               than one line at a time, so its internal static state is
               never shared across nested/concurrent calls, and unlike
               strtok_r it's plain ISO C99 - no feature-test-macro dance
               needed to see its declaration on every libc (glibc hides
               strtok_r under _POSIX_C_SOURCE when built with strict
               -std=c99, which this project's Makefiles use). */
            for (tok = strtok(s, " \t\r\n"); tok && poly_count < 32;
                 tok = strtok(NULL, " \t\r\n")) {
                int vi, ti, ni;
                parse_face_vert(tok, positions.count, uvs.count, normals.count, &vi, &ti, &ni);
                poly_v[poly_count] = vi;
                poly_t[poly_count] = ti;
                poly_n[poly_count] = ni;
                ++poly_count;
            }

            {
                int t;
                for (t = 1; t + 1 < poly_count; ++t) {
                    int tri[3]; int ttx[3]; int tn[3]; int k;
                    tri[0] = poly_v[0];   ttx[0] = poly_t[0];   tn[0] = poly_n[0];
                    tri[1] = poly_v[t];   ttx[1] = poly_t[t];   tn[1] = poly_n[t];
                    tri[2] = poly_v[t+1]; ttx[2] = poly_t[t+1]; tn[2] = poly_n[t+1];

                    /* geometric normal as fallback */
                    Vec3 gn = vec3(0,1,0);
                    if (tri[0] >= 0 && tri[1] >= 0 && tri[2] >= 0 &&
                        tri[0] < positions.count && tri[1] < positions.count &&
                        tri[2] < positions.count) {
                        Vec3 a = positions.data[tri[0]];
                        Vec3 b = positions.data[tri[1]];
                        Vec3 c = positions.data[tri[2]];
                        gn = vec3_norm(vec3_cross(vec3_sub(b,a), vec3_sub(c,a)));
                    }

                    for (k = 0; k < 3; ++k) {
                        MeshVertex mv;
                        if (tri[k] < 0 || tri[k] >= positions.count) continue;
                        mv.pos = positions.data[tri[k]];
                        if (tn[k] >= 0 && tn[k] < normals.count)
                            mv.normal = normals.data[tn[k]];
                        else
                            mv.normal = gn;
                        if (ttx[k] >= 0 && ttx[k] < uvs.count) {
                            mv.u = uvs.data[ttx[k]].x;
                            mv.v = uvs.data[ttx[k]].y;
                        } else {
                            mv.u = 0.0f;
                            mv.v = 0.0f;
                        }
                        mv.mtl = cur_mtl;
                        verts_push(m, mv, &verts_cap);
                    }
                }
            }
        }
    }

    fclose(f);
    free(positions.data);
    free(normals.data);
    free(uvs.data);

    if (m->vert_count == 0) { mesh_free(m); return 0; }

    /* compute bounds */
    {
        int i;
        m->bounds_min = m->verts[0].pos;
        m->bounds_max = m->verts[0].pos;
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
    return 1;
}

void mesh_recenter_to_ground(Mesh *m) {
    Vec3 shift;
    int i;
    if (m->vert_count == 0) return;

    shift.x = -0.5f * (m->bounds_min.x + m->bounds_max.x);
    shift.y = -m->bounds_min.y;
    shift.z = -0.5f * (m->bounds_min.z + m->bounds_max.z);

    for (i = 0; i < m->vert_count; ++i)
        m->verts[i].pos = vec3_add(m->verts[i].pos, shift);

    m->bounds_min = vec3_add(m->bounds_min, shift);
    m->bounds_max = vec3_add(m->bounds_max, shift);
}

void mesh_free(Mesh *m) {
    free(m->verts);
    m->verts = NULL;
    m->vert_count = 0;
    m->material_count = 0;
}

#endif /* PLATFORM_PC || PLATFORM_GC */
