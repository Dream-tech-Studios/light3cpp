/*
 * app.h - top-level scene state machine.
 *
 * The window/framebuffer is shared across three scenes: pick a character,
 * pick a vehicle, then race with both. The App owns the character/vehicle
 * definition lists (loaded once), the cached picks, the carousel + its
 * preview models while selecting, and the Game while racing. main.c /
 * main_gc.c drive it with app_init/app_step/app_render/app_shutdown instead
 * of talking to game_* directly.
 *
 *   CHARACTER_SELECT --confirm--> VEHICLE_SELECT --confirm--> RACE
 *          ^                            |                       |
 *          |                          back                  finished+confirm
 *          +----------------------------+-----------------------+
 */
#ifndef APP_H
#define APP_H

#include "input.h"
#include "carousel.h"
#include "vehicle.h"
#include "character.h"
#include "game.h"

typedef enum {
    SCENE_CHARACTER_SELECT,
    SCENE_VEHICLE_SELECT,
    SCENE_RACE
} SceneId;

typedef struct {
    SceneId       scene;
    int           fb_w, fb_h;

    CharacterList characters;   /* assets/characters.json */
    VehicleList   vehicles;     /* assets/vehicles.json */

    int           selected_character_id;
    int           selected_vehicle_id;

    /* Working set for the active select scene: one preview ModelInstance per
       list entry, plus the CarouselItems pointing at them. Allocated on scene
       enter, freed on scene leave. */
    Carousel       carousel;
    ModelInstance *previews;
    CarouselItem  *items;
    int            item_count;

    Game           game;         /* valid only while scene == SCENE_RACE */
    int            game_active;
    int            prev_confirm; /* edge detection for leaving a finished race */
} App;

void app_init(App *a, int fb_w, int fb_h);
void app_step(App *a, const KartInput *in, float dt);
void app_render(App *a, int fb_w, int fb_h);
void app_shutdown(App *a);

#endif /* APP_H */
