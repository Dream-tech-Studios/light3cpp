/*
 * prim.h - procedural primitive meshes (slope, sphere) as plain `Mesh`es.
 *
 * The level editor lets a designer drop cubes/slopes/spheres into a level;
 * cubes are just axis-aligned platforms (see level.h), but slopes and
 * spheres need real geometry. Rather than teach the renderer new shapes
 * (which would mean touching both render_gl.c AND render_gx.c), we bake each
 * one into the same de-indexed `Mesh` the OBJ/DAE loaders produce, so the
 * existing render_draw_mesh() draws it and track_add_mesh() can bake it into
 * collision - one code path, both platforms.
 *
 * The generated size is baked straight into the vertices (not applied as a
 * scale later), so they render/collide with render_draw_mesh(..., scale=1).
 */
#ifndef PRIM_H
#define PRIM_H

#include "obj_loader.h"  /* Mesh */

/*
 * Build a right-triangular-prism ramp centered on the origin: it sits within
 * [-half, +half], low along -Z and rising to full height along +Z. Tinted
 * (r,g,b). Allocates m->verts (free with mesh_free). Returns 1 on success.
 */
int prim_build_slope(Mesh *m, Vec3 half, float r, float g, float b);

/*
 * Build a UV-sphere centered on the origin with the given per-axis radii,
 * tinted (r,g,b). Allocates m->verts (free with mesh_free). Returns 1.
 */
int prim_build_sphere(Mesh *m, Vec3 radii, float r, float g, float b);

#endif /* PRIM_H */
