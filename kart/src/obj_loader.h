/*
 * obj_loader.h - minimal Wavefront OBJ loader -> Mesh.
 *
 * Parses positions (v), normals (vn), texture coordinates (vt) and faces
 * (f), grouped by material (usemtl). Faces are fan-triangulated, and the
 * result is a flat, de-indexed triangle list (3 vertices per triangle)
 * which is trivial to feed to immediate-mode GL on desktop and to GX
 * display lists on GameCube.
 */
#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "math3d.h"

#define MESH_MAX_MATERIALS 16

typedef struct {
    char  name[64];
    float r, g, b;
} MeshMaterial;

typedef struct {
    Vec3  pos;
    Vec3  normal;
    float u, v;   /* texture coords, v already flipped so (0,0) = top-left,
                     matching image_load.h's/baked textures' row order */
    int   mtl;    /* index into Mesh.materials */
} MeshVertex;

typedef struct {
    MeshVertex  *verts;       /* 3 per triangle */
    int          vert_count;  /* total vertices (= 3 * triangles) */

    MeshMaterial materials[MESH_MAX_MATERIALS];
    int          material_count;

    Vec3         bounds_min;
    Vec3         bounds_max;
} Mesh;

/*
 * Load an OBJ file from disk at runtime. Only available on platforms with a
 * filesystem (PLATFORM_PC); see obj_loader.c. Returns 1 on success.
 *
 * The GameCube build instead uses a mesh baked to a C header offline (see
 * tools/obj2c.c), since it has no filesystem to load vehicle models from
 * at runtime. Both paths produce the same `Mesh` type, so render_draw_mesh()
 * and everything else in game.c/vehicle.c is identical across platforms.
 */
int  mesh_load_obj(Mesh *m, const char *path);

/* Shift the mesh so it is centered on X/Z and its lowest point sits at y=0,
   so it can be placed directly on the ground. Updates bounds. */
void mesh_recenter_to_ground(Mesh *m);

void mesh_free(Mesh *m);

/* Largest dimension of the (current) bounding box; handy for auto-scaling.
   Header-only (pure function on the bounds fields) so it works for a baked
   Mesh too, without needing obj_loader.c on platforms without a filesystem. */
static inline float mesh_max_dimension(const Mesh *m) {
    float dx = m->bounds_max.x - m->bounds_min.x;
    float dy = m->bounds_max.y - m->bounds_min.y;
    float dz = m->bounds_max.z - m->bounds_min.z;
    float mx = dx;
    if (dy > mx) mx = dy;
    if (dz > mx) mx = dz;
    return mx;
}

#endif /* OBJ_LOADER_H */
