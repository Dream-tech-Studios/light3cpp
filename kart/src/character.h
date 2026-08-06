/*
 * character.h - data-driven character definitions (name/id/model/texture)
 * and the loaded runtime instance built from one. Mirrors vehicle.h, minus
 * the handling stats (characters are cosmetic for now), and shares the same
 * OBJ/PNG loading path via model_load.h.
 *
 * Definitions come from assets/characters.json, read at startup on both
 * desktop and GameCube; adding a character is just a new JSON entry plus its
 * .obj/.png, no code changes.
 */
#ifndef CHARACTER_H
#define CHARACTER_H

#include "model_load.h"

#define CHARACTER_MAX_NAME 64
#define CHARACTER_MAX_PATH MODEL_MAX_PATH

/* Target height in world units; every character mesh is auto-scaled to this
   so differently-authored models end up a consistent size on the kart. */
#define CHARACTER_TARGET_HEIGHT 1.7f

typedef struct {
    int   id;
    char  name[CHARACTER_MAX_NAME];
    char  model_path[CHARACTER_MAX_PATH];
    char  texture_path[CHARACTER_MAX_PATH];
    float yaw_offset_deg;
} CharacterDef;

typedef struct {
    CharacterDef *defs;
    int           count;
} CharacterList;

/* See vehicle_list_load_json: same contract, reading characters.json. */
int  character_list_load_json(CharacterList *list, const char *json_path);
void character_list_free(CharacterList *list);

/* NULL if no entry has this id. */
const CharacterDef *character_list_find(const CharacterList *list, int id);

/* A fully loaded, ready-to-render character (mesh + texture + placement). */
typedef ModelInstance CharacterInstance;

void character_instance_load(CharacterInstance *ci, const CharacterDef *def);
void character_instance_free(CharacterInstance *ci);

#endif /* CHARACTER_H */
