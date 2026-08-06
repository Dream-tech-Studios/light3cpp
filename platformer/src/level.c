/*
 * level.c - see level.h. Generic Level builder/utility functions only -
 * no course layouts live here, see src/levels/ for those.
 */
#include <string.h>
#include "level.h"

void level_begin(Level *lvl, const char *name) {
    lvl->platform_count    = 0;
    lvl->collectible_count = 0;
    lvl->object_count      = 0;
    strncpy(lvl->name, name, LEVEL_MAX_NAME - 1);
    lvl->name[LEVEL_MAX_NAME - 1] = '\0';
}

static LevelObject *add_object(Level *lvl, LevelObjectKind kind, Vec3 pos, float yaw) {
    LevelObject *o;
    if (lvl->object_count >= LEVEL_MAX_OBJECTS) return NULL;
    o = &lvl->objects[lvl->object_count++];
    o->kind  = kind;
    o->pos   = pos;
    o->yaw   = yaw;
    o->size  = vec3(1.0f, 1.0f, 1.0f);
    o->scale = 1.0f;
    o->r = o->g = o->b = 1.0f;
    o->model_path[0] = '\0';
    return o;
}

void level_add_slope(Level *lvl, Vec3 pos, float yaw, Vec3 half, float r, float g, float b) {
    LevelObject *o = add_object(lvl, LEVEL_OBJ_SLOPE, pos, yaw);
    if (!o) return;
    o->size = half;
    o->r = r; o->g = g; o->b = b;
}

void level_add_sphere(Level *lvl, Vec3 pos, float yaw, Vec3 radii, float r, float g, float b) {
    LevelObject *o = add_object(lvl, LEVEL_OBJ_SPHERE, pos, yaw);
    if (!o) return;
    o->size = radii;
    o->r = r; o->g = g; o->b = b;
}

void level_add_mesh(Level *lvl, Vec3 pos, float yaw, float scale, const char *model_rel) {
    LevelObject *o = add_object(lvl, LEVEL_OBJ_MESH, pos, yaw);
    if (!o) return;
    o->scale = scale;
    strncpy(o->model_path, model_rel, LEVEL_OBJ_PATH - 1);
    o->model_path[LEVEL_OBJ_PATH - 1] = '\0';
}

void level_add_platform(Level *lvl, Vec3 center, float half_x, float half_z, float visual_half_y) {
    Platform *p;
    if (lvl->platform_count >= LEVEL_MAX_PLATFORMS) return;
    p = &lvl->platforms[lvl->platform_count++];
    p->center = center;
    p->half_x = half_x;
    p->half_z = half_z;
    p->visual_half_y = visual_half_y;
}

void level_add_collectible(Level *lvl, Vec3 pos) {
    Collectible *c;
    if (lvl->collectible_count >= LEVEL_MAX_COLLECTIBLES) return;
    c = &lvl->collectibles[lvl->collectible_count++];
    c->pos = pos;
    c->collected = 0;
}

void level_set_start(Level *lvl, Vec3 pos, float heading) {
    lvl->start_pos     = pos;
    lvl->start_heading = heading;
}

void level_set_goal(Level *lvl, Vec3 pos, float radius) {
    lvl->goal_pos    = pos;
    lvl->goal_radius = radius;
}

void level_build_collision(const Level *lvl, CollisionMesh *cm) {
    int i;
    cm->tris  = NULL;
    cm->count = 0;
    for (i = 0; i < lvl->platform_count; ++i) {
        const Platform *p = &lvl->platforms[i];
        track_add_platform_top(cm, p->center, p->half_x, p->half_z);
    }
}

int level_collect_near(Level *lvl, Vec3 pos, float radius) {
    int i, n = 0;
    for (i = 0; i < lvl->collectible_count; ++i) {
        Collectible *c = &lvl->collectibles[i];
        if (!c->collected && vec3_len(vec3_sub(pos, c->pos)) <= radius) {
            c->collected = 1;
            ++n;
        }
    }
    return n;
}
