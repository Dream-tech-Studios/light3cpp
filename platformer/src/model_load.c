/*
 * model_load.c - see model_load.h.
 *
 * Reading OBJ/PNG files needs a filesystem and a heap: both platforms have
 * that (desktop via the OS, GameCube via the "gcdvd:" device from gcdvd.c),
 * so this file builds for PLATFORM_PC and PLATFORM_GC alike.
 */
#if defined(PLATFORM_PC) || defined(PLATFORM_GC)

#include <stdio.h>
#include <string.h>
#include "model_load.h"
#include "dae_loader.h"
#include "image_load.h"
#include "math3d.h"

static void with_asset_root(char *out, size_t out_size, const char *rel_path) {
    snprintf(out, out_size, "%s%s", ASSET_ROOT, rel_path);
}

/* Picks the loader by file extension so callers (character.c) don't need
   to care whether a given asset is an OBJ or a COLLADA file. */
static int has_suffix(const char *s, const char *suffix) {
    size_t ls = strlen(s), lf = strlen(suffix);
    return ls >= lf && strcmp(s + (ls - lf), suffix) == 0;
}

void model_instance_load(ModelInstance *mi,
                         const char *model_rel, const char *texture_rel,
                         float yaw_offset_deg, float target_length,
                         const char *log_tag, const char *log_what) {
    char path[sizeof(ASSET_ROOT) + MODEL_MAX_PATH];

    memset(mi, 0, sizeof(*mi));

    with_asset_root(path, sizeof(path), model_rel);
    mi->has_mesh = has_suffix(model_rel, ".dae")
                       ? mesh_load_dae(&mi->mesh, path)
                       : mesh_load_obj(&mi->mesh, path);
    if (!mi->has_mesh) {
        fprintf(stderr, "[%s] WARNING: could not load model '%s' for \"%s\" - "
                       "falling back to a placeholder box\n",
               log_tag, path, log_what);
        mi->scale   = 1.0f;
        mi->yaw_off = 0.0f;
        return;
    }
    mesh_recenter_to_ground(&mi->mesh);

    {
        float maxd = mesh_max_dimension(&mi->mesh);
        mi->scale = (maxd > 1e-4f) ? (target_length / maxd) : 1.0f;
    }
    mi->yaw_off = m3_deg2rad(yaw_offset_deg);

    if (texture_rel && texture_rel[0] != '\0') {
        int tw = 0, th = 0;
        unsigned char *pixels;
        with_asset_root(path, sizeof(path), texture_rel);
        pixels = image_load_rgba(path, &tw, &th);
        if (pixels) {
            mi->has_texture = render_texture_create(&mi->texture, pixels, tw, th);
            image_free(pixels);
        } else {
            fprintf(stderr, "[%s] WARNING: could not load texture '%s' for \"%s\" - "
                           "rendering with flat material colors instead\n",
                   log_tag, path, log_what);
        }
    }

    fprintf(stderr, "[%s] loaded \"%s\": %d verts (%d tris), texture=%s, scale=%.3f\n",
           log_tag, log_what, mi->mesh.vert_count, mi->mesh.vert_count / 3,
           mi->has_texture ? "yes" : "no", mi->scale);
}

void model_instance_free(ModelInstance *mi) {
    if (mi->has_mesh) mesh_free(&mi->mesh);
    if (mi->has_texture) render_texture_destroy(&mi->texture);
    mi->has_mesh = 0;
    mi->has_texture = 0;
}

#endif /* PLATFORM_PC || PLATFORM_GC */
