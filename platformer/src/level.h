/*
 * level.h - a 3D-platformer level: a run of platforms to hop across, a few
 * collectibles along the way, and a goal to reach.
 *
 * The actual course layouts do NOT live here - each level is its own small
 * .c/.h pair under src/levels/ (see src/levels.h for the registry that ties
 * them all together). This header only defines the shared `Level` data and
 * the small "builder" helper functions every level file calls to fill one
 * in, plus the generic operations game.c needs regardless of which level is
 * loaded (baking collision, checking collectible pickups).
 *
 * Adding a new level does NOT require touching this file - see
 * src/levels.h for the three steps involved.
 *
 * Deliberately decoupled from physics.h/Player (like track.h is), so a
 * future real level - loaded from an OBJ authored in Blender, with
 * platforms/collectibles/goal placed by a level designer - is a drop-in
 * replacement: game.c only needs the CollisionMesh this bakes plus the
 * platform/collectible list for drawing, regardless of how the Level was
 * built.
 */
#ifndef LEVEL_H
#define LEVEL_H

#include "math3d.h"
#include "track.h"

#define LEVEL_MAX_PLATFORMS    32
#define LEVEL_MAX_COLLECTIBLES 16
#define LEVEL_MAX_OBJECTS      32
#define LEVEL_MAX_NAME         48
#define LEVEL_OBJ_PATH         128

/* An axis-aligned walkable slab. `center.y` is the walkable top surface;
   `visual_half_y` is purely cosmetic (how tall the drawn box looks below
   that surface) and plays no part in collision. */
typedef struct {
    Vec3  center;
    float half_x;
    float half_z;
    float visual_half_y;
} Platform;

typedef struct {
    Vec3 pos;
    int  collected;
} Collectible;

/* A non-platform piece of level geometry: a ramp, a ball, or a model
   dropped in from an .obj/.dae. Unlike Platform (an axis-aligned slab), an
   object can be freely rotated about Y and is rendered + collided as a real
   mesh (procedurally built for SLOPE/SPHERE - see prim.h - or loaded from
   `model_path` for MESH). game.c turns each of these into a runtime mesh at
   level load; see game.c's build_props / level_build_collision usage. */
typedef enum {
    LEVEL_OBJ_SLOPE,
    LEVEL_OBJ_SPHERE,
    LEVEL_OBJ_MESH
} LevelObjectKind;

typedef struct {
    LevelObjectKind kind;
    Vec3  pos;                     /* world position of the mesh's local origin */
    float yaw;                     /* rotation about Y, radians */
    Vec3  size;                    /* SLOPE: half-extents; SPHERE: radii (unused for MESH) */
    float scale;                   /* MESH: uniform scale (unused for SLOPE/SPHERE) */
    float r, g, b;                 /* tint for SLOPE/SPHERE (MESH uses its own materials) */
    char  model_path[LEVEL_OBJ_PATH]; /* MESH only: asset-relative path (e.g. "assets/props/x.obj") */
} LevelObject;

typedef struct {
    char        name[LEVEL_MAX_NAME]; /* shown on the HUD, e.g. "1. First Steps" */

    Platform    platforms[LEVEL_MAX_PLATFORMS];
    int         platform_count;
    Collectible collectibles[LEVEL_MAX_COLLECTIBLES];
    int         collectible_count;
    LevelObject objects[LEVEL_MAX_OBJECTS];
    int         object_count;

    Vec3  start_pos;
    float start_heading;   /* matches physics.c's heading convention */
    Vec3  goal_pos;
    float goal_radius;
} Level;

/* ---- builder helpers: every level file in src/levels/ calls these ---- */

/* Resets `lvl` to empty and sets its display name. Call this first. */
void level_begin(Level *lvl, const char *name);

/* Appends one walkable platform. See `Platform` above for field meanings. */
void level_add_platform(Level *lvl, Vec3 center, float half_x, float half_z, float visual_half_y);

/* Appends one uncollected collectible at `pos`. */
void level_add_collectible(Level *lvl, Vec3 pos);

/* Appends a ramp centered at `pos` (rotated `yaw` about Y), sized by the
   half-extents `half` (low along -Z, rising to full height along +Z before
   rotation), tinted (r,g,b). Rendered and collided as a real mesh. */
void level_add_slope(Level *lvl, Vec3 pos, float yaw, Vec3 half, float r, float g, float b);

/* Appends a ball centered at `pos` with per-axis radii `radii`, tinted
   (r,g,b). Rendered and collided as a real mesh. */
void level_add_sphere(Level *lvl, Vec3 pos, float yaw, Vec3 radii, float r, float g, float b);

/* Appends a model loaded from `model_rel` (asset-relative, .obj or .dae) at
   `pos`, rotated `yaw` about Y and uniformly scaled by `scale`. Rendered and
   collided as a real mesh. */
void level_add_mesh(Level *lvl, Vec3 pos, float yaw, float scale, const char *model_rel);

/* Sets where the player spawns/respawns and which way they face
   (radians, matching player_forward's convention: 0 = facing -Z). */
void level_set_start(Level *lvl, Vec3 pos, float heading);

/* Sets the goal position and how close (world units) the player must get
   to it to complete the level. */
void level_set_goal(Level *lvl, Vec3 pos, float radius);

/* ---- generic operations, independent of which level is loaded ---- */

/* Bakes every platform's top face into `cm` (see track_add_platform_top).
   This is the *entire* collision mesh for the level - there's no separate
   flat ground plane, so stepping off a platform's edge means falling into
   the void (see PLAYER_FALL_RESET_Y in game.c). */
void level_build_collision(const Level *lvl, CollisionMesh *cm);

/* Marks any not-yet-collected item within `radius` of `pos` as collected.
   Returns how many were newly collected this call (usually 0). */
int level_collect_near(Level *lvl, Vec3 pos, float radius);

#endif /* LEVEL_H */
