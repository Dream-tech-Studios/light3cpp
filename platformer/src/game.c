/*
 * game.c - ties input, physics, the level and rendering together.
 *
 * Keeps no platform code: it talks to the renderer through render.h, so
 * the same game.c compiles for desktop and GameCube.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "game.h"
#include "render.h"
#include "hud.h"
#include "math3d.h"
#include "prim.h"
#include "dae_loader.h"
#include "model_load.h"  /* ASSET_ROOT */

/* Falling below this height (off the edge of every platform) respawns the
   player back at the level start - there's no floor under the level, only
   the platforms themselves (see level_build_collision). */
#define PLAYER_FALL_RESET_Y -20.0f

/* Third-person follow camera placement, relative to the player. */
#define CAM_BEHIND   7.0f
#define CAM_HEIGHT   4.5f
#define CAM_LOOK_Y   1.2f

/* Cosmetic bob/spin for uncollected collectibles. */
#define COLLECTIBLE_HALF_SIZE 0.35f
#define COLLECTIBLE_BOB_AMPL  0.15f
#define COLLECTIBLE_BOB_HZ    1.3f
#define COLLECTIBLE_SPIN_RATE 2.0f

/* Placeholder avatar used when the character's model fails to load. */
#define FALLBACK_HALF_EXTENTS vec3(0.35f, 0.55f, 0.25f)
#define FALLBACK_Y_OFFSET     0.55f

static void respawn(Game *g) {
    player_init(&g->player, g->level.start_pos, g->level.start_heading);
}

static int has_suffix(const char *s, const char *suffix) {
    size_t ls = strlen(s), lf = strlen(suffix);
    return ls >= lf && strcmp(s + (ls - lf), suffix) == 0;
}

/* Load a level MESH object's model (asset-relative path), recentered so its
   base sits on the ground, dispatching by extension like model_load.c. */
static int load_object_mesh(Mesh *m, const char *model_rel) {
    char path[sizeof(ASSET_ROOT) + LEVEL_OBJ_PATH];
    int ok;
    snprintf(path, sizeof(path), "%s%s", ASSET_ROOT, model_rel);
    ok = has_suffix(model_rel, ".dae") ? mesh_load_dae(m, path) : mesh_load_obj(m, path);
    if (ok) mesh_recenter_to_ground(m);
    return ok;
}

static void free_props(Game *g) {
    int i;
    for (i = 0; i < g->prop_count; ++i)
        if (g->props[i].has_mesh) mesh_free(&g->props[i].mesh);
    g->prop_count = 0;
}

/* Turn every level.object into a runtime LevelProp mesh (procedural slope/
   sphere or a loaded model). Ones that fail to build are skipped. */
static void build_props(Game *g) {
    int i;
    free_props(g);
    for (i = 0; i < g->level.object_count; ++i) {
        const LevelObject *o = &g->level.objects[i];
        LevelProp *p = &g->props[g->prop_count];
        p->has_mesh = 0;
        p->pos = o->pos;
        p->yaw = o->yaw;
        p->scale = 1.0f;
        switch (o->kind) {
        case LEVEL_OBJ_SLOPE:
            p->has_mesh = prim_build_slope(&p->mesh, o->size, o->r, o->g, o->b);
            break;
        case LEVEL_OBJ_SPHERE:
            p->has_mesh = prim_build_sphere(&p->mesh, o->size, o->r, o->g, o->b);
            break;
        case LEVEL_OBJ_MESH:
            p->has_mesh = load_object_mesh(&p->mesh, o->model_path);
            p->scale = o->scale;
            if (!p->has_mesh)
                fprintf(stderr, "[game] WARNING: level object model '%s' failed to load\n",
                        o->model_path);
            break;
        }
        if (p->has_mesh) ++g->prop_count;
    }
}

void game_load_level(Game *g, int level_index) {
    int i;
    if (level_index < 0) level_index = 0;
    if (level_index >= LEVEL_COUNT) level_index = LEVEL_COUNT - 1;
    g->level_index = level_index;

    track_free(&g->collision); /* frees the previous level's mesh, if any */
    g_levels[level_index].build(&g->level);
    build_props(g);

    /* Collision = platform tops + every object's baked triangles, so what
       you see is what you stand on / walk up (see track_add_mesh). */
    level_build_collision(&g->level, &g->collision);
    for (i = 0; i < g->prop_count; ++i)
        track_add_mesh(&g->collision, &g->props[i].mesh,
                       g->props[i].pos, g->props[i].yaw, g->props[i].scale);

    player_init(&g->player, g->level.start_pos, g->level.start_heading);
    objective_init(&g->objective, &g->level);

    {
        /* Start the follow camera already tucked in behind the player
           (facing the same way the level put them) instead of snapping in
           from wherever it last was (which could be a completely different
           part of the world on a level change) or from a fixed world-space
           point during the "ready" pause. */
        Vec3 fwd0 = player_forward(&g->player);
        g->cam_pos = vec3_add(g->player.pos, vec3(-fwd0.x * CAM_BEHIND, CAM_HEIGHT, -fwd0.z * CAM_BEHIND));
    }
}

void game_init(Game *g, const CharacterDef *character_def, int start_level_index) {
    if (character_def) {
        character_instance_load(&g->character, character_def);
    } else {
        g->character.has_mesh = 0;
        g->character.has_texture = 0;
        g->character.scale = 1.0f;
        g->character.yaw_off = 0.0f;
    }

    g->collision.tris  = NULL; /* so the first game_load_level's track_free is a no-op */
    g->collision.count = 0;
    g->prop_count = 0;         /* so the first build_props' free_props is a no-op */
    g->anim_time = 0.0f;
    g->ready = 1;
    game_load_level(g, start_level_index);
}

void game_shutdown(Game *g) {
    if (!g->ready) return;
    character_instance_free(&g->character);
    free_props(g);
    track_free(&g->collision);
    g->ready = 0;
}

void game_step(Game *g, const PlayerInput *in, float dt) {
    Player *p = &g->player;
    Vec3 fwd, desired;
    const PlayerInput *player_in = in;
    PlayerInput frozen;

    /* Input is frozen (but physics still runs, so the player settles under
       gravity) during the "ready" pause and after the level is complete. */
    if (g->objective.state != OBJECTIVE_PLAYING) {
        frozen.move = 0.0f;
        frozen.turn = 0.0f;
        frozen.jump = 0;
        frozen.quit = in->quit;
        player_in = &frozen;
    }

    player_update(p, player_in, &g->collision, dt);

    if (p->pos.y < PLAYER_FALL_RESET_Y) {
        respawn(g);
    }

    objective_update(&g->objective, p->pos, dt);

    /* Follow camera: sit behind and above the player, look at them.
       Smooth toward the target so it eases through turns and jumps. */
    fwd = player_forward(p);
    desired = vec3_add(p->pos, vec3(-fwd.x * CAM_BEHIND, CAM_HEIGHT, -fwd.z * CAM_BEHIND));

    {
        /* exponential smoothing, framerate-independent */
        float a = 1.0f - powf(0.0015f, dt);
        g->cam_pos = vec3_lerp(g->cam_pos, desired, a);
    }

    g->anim_time += dt;
}

static void draw_platforms(const Level *lvl) {
    int i;
    for (i = 0; i < lvl->platform_count; ++i) {
        const Platform *pl = &lvl->platforms[i];
        Vec3 half = vec3(pl->half_x, pl->visual_half_y, pl->half_z);
        Vec3 pos  = vec3(pl->center.x, pl->center.y - pl->visual_half_y, pl->center.z);
        render_draw_box(pos, half, 0.0f, 0.45f, 0.62f, 0.30f);
    }
}

static void draw_props(const Game *g) {
    int i;
    for (i = 0; i < g->prop_count; ++i) {
        const LevelProp *p = &g->props[i];
        if (!p->has_mesh) continue;
        render_draw_mesh(&p->mesh, p->pos, p->yaw, p->scale, NULL);
    }
}

static void draw_collectibles(const Level *lvl, float t) {
    int i;
    for (i = 0; i < lvl->collectible_count; ++i) {
        const Collectible *c = &lvl->collectibles[i];
        Vec3 pos;
        float bob;
        if (c->collected) continue;
        bob = sinf(t * COLLECTIBLE_BOB_HZ * 2.0f * M3_PI) * COLLECTIBLE_BOB_AMPL;
        pos = vec3(c->pos.x, c->pos.y + bob, c->pos.z);
        render_draw_box(pos, vec3(COLLECTIBLE_HALF_SIZE, COLLECTIBLE_HALF_SIZE, COLLECTIBLE_HALF_SIZE),
                        t * COLLECTIBLE_SPIN_RATE, 1.0f, 0.85f, 0.15f);
    }
}

/* A simple two-box flagpole marking the goal. */
static void draw_goal(Vec3 goal_pos) {
    Vec3 pole_pos = vec3(goal_pos.x, goal_pos.y + 1.5f, goal_pos.z);
    Vec3 flag_pos = vec3(goal_pos.x + 0.35f, goal_pos.y + 2.7f, goal_pos.z);
    render_draw_box(pole_pos, vec3(0.08f, 1.5f, 0.08f), 0.0f, 0.75f, 0.75f, 0.78f);
    render_draw_box(flag_pos, vec3(0.35f, 0.22f, 0.04f), 0.0f, 0.95f, 0.25f, 0.20f);
}

void game_render(Game *g, int fb_w, int fb_h) {
    Player *p = &g->player;
    Vec3 look = vec3_add(p->pos, vec3(0.0f, CAM_LOOK_Y, 0.0f));

    render_begin_frame();
    render_set_camera(g->cam_pos, look, vec3(0, 1, 0), 60.0f);

    draw_platforms(&g->level);
    draw_props(g);
    draw_collectibles(&g->level, g->anim_time);
    draw_goal(g->level.goal_pos);

    /* mat4_rotate_y(angle) sweeps -Z toward -X as angle increases, but
       player_forward(heading) sweeps -Z toward +X as heading increases
       (see physics.c) - the two rotate in opposite directions. Negating
       heading here re-aligns the mesh's rotation with the player's actual
       turning direction; character.yaw_off then corrects for whichever way
       the raw model geometry happens to face (see "yaw_offset_deg" in
       assets/characters.json). */
    if (g->character.has_mesh) {
        render_draw_mesh(&g->character.mesh, p->pos, g->character.yaw_off - p->heading,
                         g->character.scale, g->character.has_texture ? &g->character.texture : NULL);
    } else {
        /* Fallback if the character's model failed to load: a placeholder box. */
        Vec3 body_pos = vec3(p->pos.x, p->pos.y + FALLBACK_Y_OFFSET, p->pos.z);
        render_draw_box(body_pos, FALLBACK_HALF_EXTENTS, p->heading, 0.20f, 0.55f, 0.90f);
    }

    hud_render(&g->objective, g->level_index, LEVEL_COUNT, fb_w, fb_h);

    render_end_frame();
}
