/*
 * character.c - see character.h. Mirrors vehicle.c (JSON list load + a thin
 * wrapper over model_load.c), just without stats.
 */
#if defined(PLATFORM_PC) || defined(PLATFORM_GC)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "character.h"
#include "json.h"

static char *read_whole_file(const char *path) {
    FILE *f = fopen(path, "rb");
    long size;
    char *buf;

    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }

    buf = (char *)malloc((size_t)size + 1);
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        free(buf);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static void copy_field(char *dst, size_t dst_size, const JsonValue *obj, const char *key) {
    const char *s = json_as_string(json_object_get(obj, key), "");
    strncpy(dst, s, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

int character_list_load_json(CharacterList *list, const char *json_path) {
    char *text;
    JsonValue *root;
    char err[128];
    int i, n;

    list->defs = NULL;
    list->count = 0;

    text = read_whole_file(json_path);
    if (!text) {
        fprintf(stderr, "[character] could not read '%s'\n", json_path);
        return 0;
    }

    root = json_parse(text, err, sizeof(err));
    free(text);
    if (!root) {
        fprintf(stderr, "[character] failed to parse '%s': %s\n", json_path, err);
        return 0;
    }
    if (json_type(root) != JSON_ARRAY) {
        fprintf(stderr, "[character] '%s' must be a JSON array of characters\n", json_path);
        json_free(root);
        return 0;
    }

    n = json_array_count(root);
    list->defs = (CharacterDef *)calloc((size_t)n, sizeof(CharacterDef));

    for (i = 0; i < n; ++i) {
        const JsonValue *entry = json_array_get(root, i);
        CharacterDef *def = &list->defs[list->count];

        if (json_type(entry) != JSON_OBJECT) continue;

        def->id = json_as_int(json_object_get(entry, "id"), -1);
        copy_field(def->name,         sizeof(def->name),         entry, "name");
        copy_field(def->model_path,   sizeof(def->model_path),   entry, "model");
        copy_field(def->texture_path, sizeof(def->texture_path), entry, "texture");
        def->yaw_offset_deg = (float)json_as_number(json_object_get(entry, "yaw_offset_deg"), 0.0);

        if (def->id < 0 || def->model_path[0] == '\0') {
            fprintf(stderr, "[character] skipping entry %d in '%s': needs at least \"id\" and \"model\"\n",
                   i, json_path);
            continue;
        }
        ++list->count;
    }

    json_free(root);
    return list->count > 0;
}

void character_list_free(CharacterList *list) {
    free(list->defs);
    list->defs = NULL;
    list->count = 0;
}

const CharacterDef *character_list_find(const CharacterList *list, int id) {
    int i;
    for (i = 0; i < list->count; ++i) {
        if (list->defs[i].id == id) return &list->defs[i];
    }
    return NULL;
}

void character_instance_load(CharacterInstance *ci, const CharacterDef *def) {
    model_instance_load(ci, def->model_path, def->texture_path,
                        def->yaw_offset_deg, CHARACTER_TARGET_HEIGHT,
                        "character", def->name);
}

void character_instance_free(CharacterInstance *ci) {
    model_instance_free(ci);
}

#endif /* PLATFORM_PC || PLATFORM_GC */
