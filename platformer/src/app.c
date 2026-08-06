/*
 * app.c - see app.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "render.h"

/* Frees whichever select scene's working set is currently populated
   (character-select's previews, or level-select's level_defs), plus the
   CarouselItems pointing into it. Safe to call unconditionally. */
static void free_previews(App *a) {
    int i;
    if (a->previews) {
        for (i = 0; i < a->item_count; ++i)
            model_instance_free(&a->previews[i]);
        free(a->previews);
        a->previews = NULL;
    }
    free(a->level_defs);
    a->level_defs = NULL;
    free(a->items);
    a->items = NULL;
    a->item_count = 0;
}

static void end_play(App *a) {
    if (a->game_active) {
        game_shutdown(&a->game);
        a->game_active = 0;
    }
}

static void enter_character_select(App *a) {
    int i, start = 0;

    end_play(a);
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

static void enter_level_select(App *a) {
    int i;

    end_play(a);
    free_previews(a);
    a->scene = SCENE_LEVEL_SELECT;
    a->item_count = LEVEL_COUNT;

    if (a->item_count > 0) {
        /* No preview model for a level - build each Level just to read its
           name (level_defs owns the string; CarouselItem.name borrows it). */
        a->level_defs = (Level *)calloc((size_t)a->item_count, sizeof(Level));
        a->items      = (CarouselItem *)calloc((size_t)a->item_count, sizeof(CarouselItem));
        for (i = 0; i < a->item_count; ++i) {
            g_levels[i].build(&a->level_defs[i]);
            a->items[i].name       = a->level_defs[i].name;
            a->items[i].model      = NULL; /* carousel draws a placeholder box instead */
            a->items[i].stat_count = 0;
        }
    }
    carousel_init(&a->carousel, "SELECT LEVEL", a->items, a->item_count, a->selected_level_index, 1);
}

static void enter_play(App *a) {
    const CharacterDef *cd = character_list_find(&a->characters, a->selected_character_id);

    free_previews(a);
    a->scene = SCENE_PLAY;
    game_init(&a->game, cd, a->selected_level_index);
    a->game_active  = 1;
    a->prev_confirm = 1; /* swallow the confirm press that launched the level */
}

void app_init(App *a, int fb_w, int fb_h) {
    memset(a, 0, sizeof(*a));
    a->fb_w = fb_w;
    a->fb_h = fb_h;

    render_init(fb_w, fb_h);

    if (!character_list_load_json(&a->characters, ASSET_ROOT "assets/characters.json"))
        fprintf(stderr, "[app] WARNING: could not load " ASSET_ROOT "assets/characters.json - "
                       "playing with a placeholder character\n");

    a->selected_character_id = (a->characters.count > 0) ? a->characters.defs[0].id : -1;
    a->selected_level_index  = 0;

    enter_character_select(a);
}

void app_step(App *a, const PlayerInput *in, float dt) {
    switch (a->scene) {
    case SCENE_CHARACTER_SELECT: {
        CarouselResult r = carousel_step(&a->carousel, in, dt);
        if (r == CAROUSEL_CONFIRM) {
            a->selected_character_id = (a->characters.count > 0)
                ? a->characters.defs[carousel_index(&a->carousel)].id : -1;
            enter_level_select(a);
        }
        break;
    }
    case SCENE_LEVEL_SELECT: {
        CarouselResult r = carousel_step(&a->carousel, in, dt);
        if (r == CAROUSEL_CONFIRM) {
            a->selected_level_index = carousel_index(&a->carousel);
            enter_play(a);
        } else if (r == CAROUSEL_BACK) {
            enter_character_select(a);
        }
        break;
    }
    case SCENE_PLAY: {
        game_step(&a->game, in, dt);
        if (a->game.objective.state == OBJECTIVE_COMPLETE && in->menu_confirm && !a->prev_confirm) {
            if (a->game.level_index + 1 < LEVEL_COUNT) {
                /* More levels to go - keep the same character, load the next. */
                game_load_level(&a->game, a->game.level_index + 1);
            } else {
                /* That was the last level - back to level select (same
                   character, pick another level or replay this one). */
                enter_level_select(a);
            }
        }
        a->prev_confirm = in->menu_confirm;
        break;
    }
    }
}

void app_render(App *a, int fb_w, int fb_h) {
    a->fb_w = fb_w;
    a->fb_h = fb_h;

    if (a->scene == SCENE_PLAY) {
        game_render(&a->game, fb_w, fb_h);
    } else {
        carousel_render(&a->carousel, fb_w, fb_h);
    }
}

void app_shutdown(App *a) {
    free_previews(a);
    end_play(a);
    character_list_free(&a->characters);
    render_shutdown();
}
