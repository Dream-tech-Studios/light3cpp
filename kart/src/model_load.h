/*
 * model_load.h - a loaded, ready-to-render model (mesh + optional texture +
 * placement tuning), plus the shared loader that builds one from an OBJ/PNG
 * pair. Used by both vehicles (vehicle.c) and characters (character.c) so the
 * "load an .obj, recenter it, auto-scale it, load its .png" logic lives in
 * exactly one place.
 */
#ifndef MODEL_LOAD_H
#define MODEL_LOAD_H

#include "obj_loader.h"
#include "render.h"

/* Prefix every asset path (JSON file, .obj, .png) with this before opening
 * it. Desktop assets are already resolved relative to the working directory
 * (see chdir_to_executable_dir() in main.c), so no prefix is needed there;
 * GameCube has to route through the "gcdvd:" device gcdvd.c registers instead
 * of the plain filesystem. */
#if defined(PLATFORM_GC)
  #define ASSET_ROOT "gcdvd:/"
#else
  #define ASSET_ROOT ""
#endif

/* Max length of an asset-relative path stored in a *Def (before ASSET_ROOT). */
#define MODEL_MAX_PATH 128

typedef struct {
    Mesh    mesh;
    Texture texture;
    int     has_mesh;
    int     has_texture;
    float   scale;    /* uniform scale so the mesh's largest dimension == target length */
    float   yaw_off;  /* constant yaw correction for the model's local facing,
                         applied the same way game.c always has: as
                         (yaw_off - heading), see game_render(). */
} ModelInstance;

/*
 * Loads an OBJ mesh (recentered so it sits on the ground and auto-scaled so
 * its largest dimension equals `target_length`) plus an optional PNG texture.
 * `model_rel` and `texture_rel` are asset-relative paths (ASSET_ROOT is
 * applied internally); `texture_rel` may be NULL or "" for an untextured
 * model. On mesh-load failure `mi` comes back with has_mesh=0 (scale=1,
 * yaw_off=0) so callers can fall back to a placeholder. `log_tag`/`log_what`
 * only flavor the diagnostic messages (e.g. "vehicle"/"Mach Bike").
 */
void model_instance_load(ModelInstance *mi,
                         const char *model_rel, const char *texture_rel,
                         float yaw_offset_deg, float target_length,
                         const char *log_tag, const char *log_what);

void model_instance_free(ModelInstance *mi);

#endif /* MODEL_LOAD_H */
