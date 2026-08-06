/*
 * character.h - data-driven character definitions (name/id/model/texture)
 * and the loaded runtime instance built from one; shares the model/texture
 * loading path (OBJ or COLLADA + PNG) via model_load.h.
 *
 * Definitions come from assets/characters.json, read at startup on both
 * desktop and GameCube; adding a character is just a new JSON entry plus
 * its model/texture, no code changes.
 */
#ifndef CHARACTER_H
#define CHARACTER_H

#include "model_load.h"

#define CHARACTER_MAX_NAME 64
#define CHARACTER_MAX_PATH MODEL_MAX_PATH

/* Target height in world units; every character mesh is auto-scaled to this
   so differently-authored models end up a consistent size in-game. */
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

/*
 * Reads a JSON array of character objects from `json_path`; each needs at
 * least "id" (number), "name" (string) and "model" (string, path to an
 * OBJ or COLLADA mesh); "texture" and "yaw_offset_deg" are optional.
 * Returns 1 on success (list has >= 1 entry), 0 on a missing/malformed
 * file. `json_path` should already include ASSET_ROOT.
 */
int  character_list_load_json(CharacterList *list, const char *json_path);
void character_list_free(CharacterList *list);

/* NULL if no entry has this id. */
const CharacterDef *character_list_find(const CharacterList *list, int id);

/* A fully loaded, ready-to-render character (mesh + texture + placement). */
typedef ModelInstance CharacterInstance;

void character_instance_load(CharacterInstance *ci, const CharacterDef *def);
void character_instance_free(CharacterInstance *ci);

#endif /* CHARACTER_H */
