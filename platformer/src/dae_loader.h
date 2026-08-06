/*
 * dae_loader.h - minimal COLLADA (.dae) static-mesh loader -> Mesh.
 *
 * Supports exactly the subset needed to pull a triangle soup + UVs out of a
 * <library_geometries> block: one or more <geometry><mesh> entries, each
 * with a POSITION source (via <vertices>), an optional NORMAL source and an
 * optional TEXCOORD source, feeding either a <polylist> or <triangles>
 * element. Every geometry found in the file is merged into one flat,
 * de-indexed triangle list, exactly like mesh_load_obj produces - so
 * render_draw_mesh and model_load.c don't need to know which loader
 * actually produced the Mesh.
 *
 * Skinning/joints/animation (<library_controllers>, <library_visual_scenes>
 * joint hierarchy) are ignored entirely: vertices are read straight out of
 * their raw bind-pose positions, so an unrigged - or rigged-but-untouched -
 * character comes out as a perfectly good static mesh, just not animatable.
 */
#ifndef DAE_LOADER_H
#define DAE_LOADER_H

#include "obj_loader.h"

/* Load a COLLADA file from disk. Returns 1 on success (see obj_loader.h's
   mesh_load_obj for the shared Mesh/MeshVertex contract). */
int mesh_load_dae(Mesh *m, const char *path);

#endif /* DAE_LOADER_H */
