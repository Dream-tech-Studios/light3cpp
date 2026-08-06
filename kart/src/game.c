/*
 * game.c - ties input, physics, track and rendering together.
 *
 * Keeps no platform code: it talks to the renderer through render.h, so
 * the same game.c compiles for desktop and GameCube.
 */
#include <stddef.h>
#include <stdio.h>
#include "game.h"
#include "render.h"
#include "hud.h"
#include "math3d.h"

#define TRACK_HALF_SIZE 120.0f
#define GROUND_Y         0.0f

#define CIRCUIT_LENGTH       90.0f
#define CIRCUIT_DEPTH        50.0f
#define CIRCUIT_TRACK_WIDTH  14.0f
#define RACE_TOTAL_LAPS       3
#define KART_COLLIDE_RADIUS   1.0f  /* circle radius used against circuit walls */

/*
 * Physics ranges the 0..10 vehicle stats map onto. Anchored so a "typical"
 * stat lands near the hand-tuned defaults kart_init uses (max_speed 28,
 * accel 22): e.g. a speed rating of ~8 and accel of ~6 reproduce roughly the
 * original feel. A stat left at 0 (no "stats" block in the JSON) keeps the
 * kart_init default for that field instead of forcing it to the minimum.
 */
#define SPEED_MIN 18.0f
#define SPEED_MAX 34.0f
#define ACCEL_MIN 14.0f
#define ACCEL_MAX 30.0f

/* Rider placement on the kart (no real rig yet): nudge the character up so
   it sits on the bike rather than through the ground. */
#define RIDER_Y_OFFSET 0.30f

static void apply_vehicle_stats(Kart *k, const VehicleDef *def) {
    if (!def) return;
    if (def->stats.speed > 0.0f)
        k->max_speed = m3_lerpf(SPEED_MIN, SPEED_MAX, def->stats.speed / 10.0f);
    if (def->stats.acceleration > 0.0f)
        k->accel = m3_lerpf(ACCEL_MIN, ACCEL_MAX, def->stats.acceleration / 10.0f);
}

void game_init(Game *g, const VehicleDef *vehicle_def, const CharacterDef *character_def) {
    track_make_plane(&g->track, TRACK_HALF_SIZE, GROUND_Y);
    circuit_make_oval(&g->circuit, CIRCUIT_LENGTH, CIRCUIT_DEPTH, CIRCUIT_TRACK_WIDTH);
    kart_init(&g->kart, g->circuit.start_pos, g->circuit.start_heading);
    apply_vehicle_stats(&g->kart, vehicle_def);
    race_init(&g->race, &g->circuit, RACE_TOTAL_LAPS, g->circuit.start_pos);

    if (vehicle_def) {
        vehicle_instance_load(&g->vehicle, vehicle_def);
    } else {
        g->vehicle.has_mesh = 0;
        g->vehicle.has_texture = 0;
        g->vehicle.scale = 1.0f;
        g->vehicle.yaw_off = 0.0f;
    }

    if (character_def) {
        character_instance_load(&g->character, character_def);
    } else {
        g->character.has_mesh = 0;
        g->character.has_texture = 0;
        g->character.scale = 1.0f;
        g->character.yaw_off = 0.0f;
    }

    {
        /* Start the chase camera already tucked in behind the kart (facing
           the same way race_init/circuit put it) instead of snapping in
           from a fixed world-space point during the countdown. */
        Vec3 fwd0 = kart_forward(&g->kart);
        g->cam_pos = vec3_add(g->kart.pos, vec3(-fwd0.x * 11.0f, 5.0f, -fwd0.z * 11.0f));
    }
    g->ready = 1;
}

void game_shutdown(Game *g) {
    if (!g->ready) return;
    vehicle_instance_free(&g->vehicle);
    character_instance_free(&g->character);
    track_free(&g->track);
    circuit_free(&g->circuit);
    g->ready = 0;
}

void game_step(Game *g, const KartInput *in, float dt) {
    Kart *k = &g->kart;
    Vec3 fwd, desired;
    const KartInput *kart_in = in;
    KartInput frozen;

    /* Input is frozen (but physics still runs, so the kart settles under
       gravity) during the start countdown and after the race finishes. */
    if (g->race.state != RACE_RUNNING) {
        frozen.throttle = 0.0f;
        frozen.brake    = 0.0f;
        frozen.steer    = 0.0f;
        frozen.drift    = 0;
        frozen.quit     = in->quit;
        kart_in = &frozen;
    }

    kart_update(k, kart_in, &g->track, dt);
    circuit_resolve_walls(&g->circuit, &k->pos, &k->vel, KART_COLLIDE_RADIUS);

    /* No in-place "gas to race again" restart anymore: once RACE_FINISHED,
       the App (app.c) watches for a confirm press and tears the race down to
       return to the select screens. */
    race_update(&g->race, k->pos, dt, 0);

    /* Chase camera: sit behind and above the kart, look at it.
       Smooth toward the target so it eases through turns. */
    fwd = kart_forward(k);
    desired = vec3_add(k->pos, vec3(-fwd.x * 11.0f, 5.0f, -fwd.z * 11.0f));

    {
        /* exponential smoothing, framerate-independent */
        float a = 1.0f - powf(0.0015f, dt);
        g->cam_pos = vec3_lerp(g->cam_pos, desired, a);
    }
}

/* Barrier wall visuals: each Wall segment (a flat XZ collision line) is
   drawn as a thin box spanning it, oriented with the same heading-style
   yaw convention physics.c/circuit.c use (see heading_from_forward in
   circuit.c) so it lines up with render_draw_box's rotation. */
#define WALL_VISUAL_HALF_THICK  0.15f
#define WALL_VISUAL_HALF_HEIGHT 0.6f

static void draw_walls(const Circuit *c) {
    int i;
    for (i = 0; i < c->wall_count; ++i) {
        Vec3  a = c->walls[i].a, b = c->walls[i].b;
        Vec3  mid = vec3_scale(vec3_add(a, b), 0.5f);
        Vec3  diff = vec3_sub(b, a);
        float len = vec3_len(diff);
        float yaw = (len > 1e-4f) ? atan2f(diff.x, -diff.z) : 0.0f;
        Vec3  pos = vec3(mid.x, GROUND_Y + WALL_VISUAL_HALF_HEIGHT, mid.z);
        Vec3  half_extents = vec3(WALL_VISUAL_HALF_THICK, WALL_VISUAL_HALF_HEIGHT, len * 0.5f);

        render_draw_box(pos, half_extents, yaw, 0.85f, 0.20f, 0.20f);
    }
}

void game_render(Game *g, int fb_w, int fb_h) {
    Kart *k = &g->kart;
    Vec3 look = vec3_add(k->pos, vec3(0.0f, 1.2f, 0.0f));

    render_begin_frame();
    render_set_camera(g->cam_pos, look, vec3(0, 1, 0), 60.0f);

    render_draw_ground(GROUND_Y, TRACK_HALF_SIZE, 4.0f);
    draw_walls(&g->circuit);

    if (g->vehicle.has_mesh) {
        /* mat4_rotate_y(angle) sweeps -Z toward -X as angle increases, but
           kart_forward(heading) sweeps -Z toward +X as heading increases
           (see physics.c) - the two rotate in opposite directions. Negating
           heading here re-aligns the mesh's rotation with the kart's actual
           turning direction; vehicle.yaw_off then corrects for whichever
           way the raw model geometry happens to face (see "yaw_offset_deg"
           in assets/vehicles.json). */
        render_draw_mesh(&g->vehicle.mesh, k->pos, g->vehicle.yaw_off - k->heading,
                         g->vehicle.scale, g->vehicle.has_texture ? &g->vehicle.texture : NULL);
    } else {
        /* Fallback if the vehicle's model failed to load: a placeholder box. */
        Vec3 body_pos = vec3(k->pos.x, k->pos.y + 0.5f, k->pos.z);
        render_draw_box(body_pos, vec3(0.7f, 0.5f, 1.1f), k->heading,
                        0.90f, 0.20f, 0.20f);
    }

    /* Rider sits on the kart, using the same (yaw_off - heading) convention
       as the vehicle so it faces the way the kart is pointing. Purely
       cosmetic - no animation. */
    if (g->character.has_mesh) {
        Vec3 rider_pos = vec3(k->pos.x, k->pos.y + RIDER_Y_OFFSET, k->pos.z);
        render_draw_mesh(&g->character.mesh, rider_pos,
                         g->character.yaw_off - k->heading, g->character.scale,
                         g->character.has_texture ? &g->character.texture : NULL);
    }

    hud_render(&g->race, fb_w, fb_h);

    render_end_frame();
}
