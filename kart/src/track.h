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

void track_free(CollisionMesh *cm);

/*
 * Raycast against the mesh. `dir` need not be normalized.
 * On hit: returns 1, sets *out_t (param along dir) and *out_normal.
 * Returns the nearest hit with t in [0, inf).
 */
int track_raycast(const CollisionMesh *cm, Vec3 origin, Vec3 dir,
                  float *out_t, Vec3 *out_normal);

/*
 * Ground query for the kart: cast straight down from above `pos`.
 * On hit: returns 1, sets *out_y (ground height) and *out_normal.
 */
int track_ground(const CollisionMesh *cm, Vec3 pos,
                 float *out_y, Vec3 *out_normal);

#endif /* TRACK_H */
