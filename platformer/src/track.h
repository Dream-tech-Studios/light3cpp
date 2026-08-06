/*
 * track.h - collision mesh and ground queries.
 *
 * A track is just a list of triangles. The default track is a flat quad
 * (two triangles), but any triangle soup works, so swapping in an OBJ
 * track mesh later requires no physics changes.
 */
#ifndef TRACK_H
#define TRACK_H

#include "math3d.h"
#include "obj_loader.h"  /* Mesh, for track_add_mesh */

typedef struct {
    Vec3 a, b, c;
    Vec3 normal;   /* precomputed, normalized */
} Triangle;

typedef struct {
    Triangle *tris;
    int       count;
} CollisionMesh;

/* Build a flat square ground plane centered at origin at height `y`. */
void track_make_plane(CollisionMesh *cm, float half_size, float y);

/*
 * Appends an axis-aligned rectangle (as two triangles, wound so the normal
 * points +Y) to an already-built mesh - e.g. a platform's top face, at
 * height `center.y` (its X/Z components place the footprint). Since
 * track_ground() always finds the *nearest* surface above hit by its
 * downward ray, stacking these into the same mesh is all a level needs to
 * get walkable floating platforms at any height, no physics changes
 * required (see level.c).
 */
void track_add_platform_top(CollisionMesh *cm, Vec3 center, float half_x, float half_z);

/*
 * Appends every triangle of `mesh` to `cm`, transformed by the same
 * translate * rotateY(yaw) * uniform-scale that render_draw_mesh() applies -
 * so whatever the player sees (a slope, a sphere, a dropped-in OBJ/DAE prop;
 * see prim.h and level.h's LevelObject) is exactly what they collide with.
 * Since track_ground() just finds the nearest surface below, no physics
 * changes are needed to stand on / walk up the baked geometry.
 */
void track_add_mesh(CollisionMesh *cm, const Mesh *mesh, Vec3 pos, float yaw, float scale);

void track_free(CollisionMesh *cm);

/*
 * Raycast against the mesh. `dir` need not be normalized.
 * On hit: returns 1, sets *out_t (param along dir) and *out_normal.
 * Returns the nearest hit with t in [0, inf).
 */
int track_raycast(const CollisionMesh *cm, Vec3 origin, Vec3 dir,
                  float *out_t, Vec3 *out_normal);

/*
 * Ground query for the player: cast straight down from above `pos`.
 * On hit: returns 1, sets *out_y (ground height) and *out_normal.
 */
int track_ground(const CollisionMesh *cm, Vec3 pos,
                 float *out_y, Vec3 *out_normal);

#endif /* TRACK_H */
