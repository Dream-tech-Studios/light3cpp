/*
 * game.h - top-level game state and the platform-facing entry points.
 *
 * The platform layer (main.c) owns the window/GL context and the timing
 * loop; it calls game_init once, then game_step at a fixed timestep and
 * game_render once per displayed frame.
 */
#ifndef GAME_H
#define GAME_H

#include "physics.h"
#include "track.h"
#include "input.h"
#include "obj_loader.h"
#include "level.h"
#include "levels.h"
#include "objective.h"
#include "character.h"

/* A level object (slope/sphere/mesh) turned into a concrete runtime mesh at
   load time: procedurally built (prim.h) for slopes/spheres, or loaded from
   disk for MESH objects. Both rendered via render_draw_mesh and baked into
   the collision mesh (see game.c). */
typedef struct {
    Mesh    mesh;
    int     has_mesh;
    Vec3    pos;
    float   yaw;
    float   scale;
} LevelProp;

typedef struct {
    Player            player;
    CollisionMesh     collision;  /* baked from level.platforms + objects, see game_load_level */
    Level             level;
    Objective         objective;
    int               level_index; /* index into g_levels[] of the loaded level */
    LevelProp         props[LEVEL_MAX_OBJECTS]; /* runtime meshes for level.objects */
    int               prop_count;
    CharacterInstance character;  /* the player's visible avatar */
    Vec3              cam_pos;    /* smoothed follow-camera position */
    float             anim_time;  /* seconds elapsed, drives collectible bob/spin */
    int               ready;
} Game;

/*
 * Sets up the game for the chosen character and loads g_levels[start_level_index]
 * (clamped into range, see game_load_level). `character_def` may be NULL
 * (falls back to a placeholder box avatar). The caller owns the def (it
 * must outlive game_shutdown) and is responsible for render_init() having
 * been called once beforehand (see app.c).
 */
void game_init(Game *g, const CharacterDef *character_def, int start_level_index);
void game_shutdown(Game *g);

/*
 * (Re)builds the level/collision/objective/player for g_levels[level_index]
 * and keeps the already-loaded character - use this to advance between
 * levels without reloading the avatar. `level_index` must be in
 * [0, LEVEL_COUNT). Safe to call any time after game_init.
 */
void game_load_level(Game *g, int level_index);

/* Fixed-timestep simulation step. */
void game_step(Game *g, const PlayerInput *in, float dt);

/* Draw the current state. `fb_w`/`fb_h` size the HUD overlay to the
   current framebuffer (matches render_set_viewport's most recent size). */
void game_render(Game *g, int fb_w, int fb_h);

#endif /* GAME_H */
