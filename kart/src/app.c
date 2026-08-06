/*
 * app.c - see app.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "render.h"

static void free_previews(App *a) {
    int i;
    if (a->previews) {
        for (i = 0; i < a->item_count; ++i)
            model_instance_free(&a->previews[i]);
        free(a->previews);
        a->previews = NULL;
    }
    free(a->items);
    a->items = NULL;
    a->item_count = 0;
}

static void set_stat(CarouselStat *s, const char *label, float value, int present) {
    strncpy(s->label, label, sizeof(s->label) - 1);
    s->label[sizeof(s->label) - 1] = '\0';
    s->value   = value;
    s->present = present;
}

static void end_race(App *a) {
    if (a->game_active) {
        game_shutdown(&a->game);
        a->game_active = 0;
    }
}

static void enter_character_select(App *a) {
    int i, start = 0;

    end_race(a);
    free_previews(a);
    a->scene = SCENE_CHARACTER_SELECT;
    a->item_count = a->characters.count;

    if (a->item_count > 0) {
        a->previews = (ModelInstance *)calloc((size_t)a->item_count, sizeof(ModelInstance));
        a->items    = (CarouselItem  *)calloc((size_t)a->item_count, sizeof(CarouselItem));
        for (i = 0; i < a->item_count; ++i) {
            const CharacterDef *d = &a->characters.defs[i];
            character_instance_load(&a->previews[i], d);
            a->items[i].name       = d->name;
            a->items[i].model      = &a->previews[i];
            a->items[i].stat_count = 0;
            if (d->id == a->selected_character_id) start = i;
        }
    }
    carousel_init(&a->carousel, "SELECT CHARACTER", a->items, a->item_count, start, 0);
}

static void enter_vehicle_select(App *a) {
    int i, start = 0;

    free_previews(a);
    a->scene = SCENE_VEHICLE_SELECT;
    a->item_count = a->vehicles.count;

    if (a->item_count > 0) {
        a->previews = (ModelInstance *)calloc((size_t)a->item_count, sizeof(ModelInstance));
        a->items    = (CarouselItem  *)calloc((size_t)a->item_count, sizeof(CarouselItem));
        for (i = 0; i < a->item_count; ++i) {
            const VehicleDef *d = &a->vehicles.defs[i];
            vehicle_instance_load(&a->previews[i], d);
            a->items[i].name  = d->name;
            a->items[i].model = &a->previews[i];
            set_stat(&a->items[i].stats[0], "SPEED",  d->stats.speed,        1);
            set_stat(&a->items[i].stats[1], "ACCEL",  d->stats.acceleration, 1);
            set_stat(&a->items[i].stats[2], "WEIGHT", d->stats.weight,       d->stats.has_weight);
            a->items[i].stat_count = 3;
            if (d->id == a->selected_vehicle_id) start = i;
        }
    }
    carousel_init(&a->carousel, "SELECT VEHICLE", a->items, a->item_count, start, 1);
}

static void enter_race(App *a) {
    const VehicleDef   *vd = vehicle_list_find(&a->vehicles, a->selected_vehicle_id);
    const CharacterDef *cd = character_list_find(&a->characters, a->selected_character_id);

    free_previews(a);
    a->scene = SCENE_RACE;
    game_init(&a->game, vd, cd);
    a->game_active  = 1;
    a->prev_confirm = 1; /* swallow the confirm press that launched the race */
}

void app_init(App *a, int fb_w, int fb_h) {
    memset(a, 0, sizeof(*a));
    a->fb_w = fb_w;
    a->fb_h = fb_h;

    render_init(fb_w, fb_h);

    if (!character_list_load_json(&a->characters, ASSET_ROOT "assets/characters.json"))
        fprintf(stderr, "[app] WARNING: could not load " ASSET_ROOT "assets/characters.json - "
                       "racing with no rider\n");
    if (!vehicle_list_load_json(&a->vehicles, ASSET_ROOT "assets/vehicles.json"))
        fprintf(stderr, "[app] WARNING: could not load " ASSET_ROOT "assets/vehicles.json - "
                       "racing with a placeholder kart\n");

    a->selected_character_id = (a->characters.count > 0) ? a->characters.defs[0].id : -1;
    a->selected_vehicle_id   = (a->vehicles.count   > 0) ? a->vehicles.defs[0].id   : -1;

    enter_character_select(a);
}

void app_step(App *a, const KartInput *in, float dt) {
    switch (a->scene) {
    case SCENE_CHARACTER_SELECT: {
        CarouselResult r = carousel_step(&a->carousel, in, dt);
        if (r == CAROUSEL_CONFIRM) {
            a->selected_character_id = (a->characters.count > 0)
                ? a->characters.defs[carousel_index(&a->carousel)].id : -1;
            enter_vehicle_select(a);
        }
        break;
    }
    case SCENE_VEHICLE_SELECT: {
        CarouselResult r = carousel_step(&a->carousel, in, dt);
        if (r == CAROUSEL_CONFIRM) {
            a->selected_vehicle_id = (a->vehicles.count > 0)
                ? a->vehicles.defs[carousel_index(&a->carousel)].id : -1;
            enter_race(a);
        } else if (r == CAROUSEL_BACK) {
            enter_character_select(a);
        }
        break;
    }
    case SCENE_RACE: {
        game_step(&a->game, in, dt);
        if (a->game.race.state == RACE_FINISHED && in->menu_confirm && !a->prev_confirm) {
            /* Clear the cached picks and go back for a fresh selection. */
            a->selected_character_id = (a->characters.count > 0) ? a->characters.defs[0].id : -1;
            a->selected_vehicle_id   = (a->vehicles.count   > 0) ? a->vehicles.defs[0].id   : -1;
            enter_character_select(a);
        }
        a->prev_confirm = in->menu_confirm;
        break;
    }
    }
}

void app_render(App *a, int fb_w, int fb_h) {
    a->fb_w = fb_w;
    a->fb_h = fb_h;

    if (a->scene == SCENE_RACE) {
        game_render(&a->game, fb_w, fb_h);
    } else {
        carousel_render(&a->carousel, fb_w, fb_h);
    }
}

void app_shutdown(App *a) {
    free_previews(a);
    end_race(a);
    vehicle_list_free(&a->vehicles);
    character_list_free(&a->characters);
    render_shutdown();
}
