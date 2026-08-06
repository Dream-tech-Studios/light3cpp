/*
 * vehicle.c - see vehicle.h.
 *
 * Reading JSON needs a filesystem and a heap: both platforms have that
 * (desktop via the OS, GameCube via the "gcdvd:" device from gcdvd.c), so
 * this file builds for PLATFORM_PC and PLATFORM_GC alike. The mesh/texture
 * loading itself lives in model_load.c, shared with characters.
 */
#if defined(PLATFORM_PC) || defined(PLATFORM_GC)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vehicle.h"
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

static void parse_stats(VehicleStats *stats, const JsonValue *entry) {
    const JsonValue *s = json_object_get(entry, "stats");
    const JsonValue *w;

    stats->speed        = 0.0f;
    stats->acceleration = 0.0f;
    stats->weight       = 0.0f;
    stats->has_weight   = 0;
    if (!s || json_type(s) != JSON_OBJECT) return;

    stats->speed        = (float)json_as_number(json_object_get(s, "speed"), 0.0);
    stats->acceleration = (float)json_as_number(json_object_get(s, "acceleration"), 0.0);

    /* "weight" is the reserved third ("null") stat: present-but-null or
       absent both leave has_weight=0 so the carousel shows it as "--". */
    w = json_object_get(s, "weight");
    if (w && json_type(w) == JSON_NUMBER) {
        stats->weight     = (float)json_as_number(w, 0.0);
        stats->has_weight = 1;
    }
}

int vehicle_list_load_json(VehicleList *list, const char *json_path) {
    char *text;
    JsonValue *root;
    char err[128];
    int i, n;

    list->defs = NULL;
    list->count = 0;

    text = read_whole_file(json_path);
    if (!text) {
        fprintf(stderr, "[vehicle] could not read '%s'\n", json_path);
        return 0;
    }

    root = json_parse(text, err, sizeof(err));
    free(text);
    if (!root) {
        fprintf(stderr, "[vehicle] failed to parse '%s': %s\n", json_path, err);
        return 0;
    }
    if (json_type(root) != JSON_ARRAY) {
        fprintf(stderr, "[vehicle] '%s' must be a JSON array of vehicles\n", json_path);
        json_free(root);
        return 0;
    }

    n = json_array_count(root);
    list->defs = (VehicleDef *)calloc((size_t)n, sizeof(VehicleDef));

    for (i = 0; i < n; ++i) {
        const JsonValue *entry = json_array_get(root, i);
        VehicleDef *def = &list->defs[list->count];

        if (json_type(entry) != JSON_OBJECT) continue;

        def->id = json_as_int(json_object_get(entry, "id"), -1);
        copy_field(def->name,         sizeof(def->name),         entry, "name");
        copy_field(def->model_path,   sizeof(def->model_path),   entry, "model");
        copy_field(def->texture_path, sizeof(def->texture_path), entry, "texture");
        def->yaw_offset_deg = (float)json_as_number(json_object_get(entry, "yaw_offset_deg"), 0.0);
        parse_stats(&def->stats, entry);

        if (def->id < 0 || def->model_path[0] == '\0') {
            fprintf(stderr, "[vehicle] skipping entry %d in '%s': needs at least \"id\" and \"model\"\n",
                   i, json_path);
            continue;
        }
        ++list->count;
    }

    json_free(root);
    return list->count > 0;
}

void vehicle_list_free(VehicleList *list) {
    free(list->defs);
    list->defs = NULL;
    list->count = 0;
}

const VehicleDef *vehicle_list_find(const VehicleList *list, int id) {
    int i;
    for (i = 0; i < list->count; ++i) {
        if (list->defs[i].id == id) return &list->defs[i];
    }
    return NULL;
}

void vehicle_instance_load(VehicleInstance *vi, const VehicleDef *def) {
    model_instance_load(vi, def->model_path, def->texture_path,
                        def->yaw_offset_deg, VEHICLE_TARGET_LENGTH,
                        "vehicle", def->name);
}

void vehicle_instance_free(VehicleInstance *vi) {
    model_instance_free(vi);
}

#endif /* PLATFORM_PC || PLATFORM_GC */
