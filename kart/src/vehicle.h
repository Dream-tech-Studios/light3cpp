/*
 * vehicle.h - data-driven vehicle definitions (name/id/model/texture/stats)
 * and the loaded runtime instance built from one.
 *
 * The definitions come from a JSON file (assets/vehicles.json) read at
 * startup on both desktop and GameCube - adding a new kart is just a new
 * JSON entry plus its .obj/.png, no code changes. On desktop, paths in
 * that JSON resolve relative to the executable's working directory; on
 * GameCube they're read off the disc itself through the "gcdvd:" device
 * gcdvd.c registers (see ASSET_ROOT in model_load.h). Either way, game.c
 * ends up with the same VehicleInstance and doesn't care which platform it
 * came from.
 */
#ifndef VEHICLE_H
#define VEHICLE_H

#include "model_load.h"

#define VEHICLE_MAX_NAME 64
#define VEHICLE_MAX_PATH MODEL_MAX_PATH

/* Desired vehicle length in world units; every vehicle is auto-scaled to
   this regardless of how its source model was authored/exported. */
#define VEHICLE_TARGET_LENGTH 3.0f

/*
 * Per-vehicle handling stats, as 0..10 ratings in assets/vehicles.json.
 * `speed` and `acceleration` are mapped onto the kart's physics at spawn
 * (see game.c); `weight` is a reserved third stat - parsed and shown on the
 * select screen but not yet wired to physics. `has_weight` is 0 when the
 * JSON value is null/absent (so the carousel can render it as "--").
 */
typedef struct {
    float speed;
    float acceleration;
    float weight;
    int   has_weight;
} VehicleStats;

typedef struct {
    int          id;
    char         name[VEHICLE_MAX_NAME];
    char         model_path[VEHICLE_MAX_PATH];
    char         texture_path[VEHICLE_MAX_PATH];
    float        yaw_offset_deg; /* per-model correction for the source mesh's local facing */
    VehicleStats stats;
} VehicleDef;

typedef struct {
    VehicleDef *defs;
    int         count;
} VehicleList;

/*
 * Reads a JSON array of vehicle objects from `json_path`; each needs at
 * least "id" (number), "name" (string), "model" (string, path to an OBJ)
 * and "texture" (string, path to a PNG); "yaw_offset_deg" and the "stats"
 * object are optional (default 0). Returns 1 on success (list has >= 1
 * entry), 0 on a missing/malformed file. `json_path` should already include
 * ASSET_ROOT.
 */
int  vehicle_list_load_json(VehicleList *list, const char *json_path);
void vehicle_list_free(VehicleList *list);

/* NULL if no entry has this id. */
const VehicleDef *vehicle_list_find(const VehicleList *list, int id);

/* A fully loaded, ready-to-render vehicle (mesh + texture + placement). */
typedef ModelInstance VehicleInstance;

/*
 * Loads the mesh + texture referenced by `def` (its model_path/texture_path
 * are plain relative paths; ASSET_ROOT is applied internally) and computes
 * scale/yaw_off. On failure to load the mesh, `vi` comes back with
 * has_mesh=0 so game.c can fall back to its placeholder box.
 */
void vehicle_instance_load(VehicleInstance *vi, const VehicleDef *def);
void vehicle_instance_free(VehicleInstance *vi);

#endif /* VEHICLE_H */
