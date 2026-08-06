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
#include "circuit.h"
#include "race.h"
#include "vehicle.h"
#include "character.h"

typedef struct {
    Kart              kart;
    CollisionMesh     track;
    Circuit           circuit;
    Race              race;
    VehicleInstance   vehicle;    /* the kart currently being driven */
    CharacterInstance character;  /* the rider sitting on it (cosmetic) */
    Vec3              cam_pos;     /* smoothed chase camera position */
    int               ready;
} Game;

/*
 * Sets up a race for the chosen vehicle + character. Either def may be NULL
 * (falls back to a placeholder box / no rider). The vehicle's speed and
 * acceleration stats are applied to the kart here. The caller owns the defs
 * (they must outlive game_shutdown) and is responsible for render_init()
 * having been called once beforehand (see app.c).
 */
void game_init(Game *g, const VehicleDef *vehicle_def, const CharacterDef *character_def);
void game_shutdown(Game *g);

/* Fixed-timestep simulation step. */
void game_step(Game *g, const KartInput *in, float dt);

/* Draw the current state. `fb_w`/`fb_h` size the HUD overlay to the
   current framebuffer (matches render_set_viewport's most recent size). */
void game_render(Game *g, int fb_w, int fb_h);

#endif /* GAME_H */
