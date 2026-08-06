/*
 * app.h - top-level scene state machine.
 *
 * The window/framebuffer is shared across three scenes: pick a character,
 * pick a level, then play. The App owns the character definition list
 * (loaded once), the cached picks, the carousel + its working set while
 * selecting, and the Game while playing. main.c / main_gc.c drive it with
 * app_init/app_step/app_render/app_shutdown instead of talking to game_*
 * directly.
 *
 *   CHARACTER_SELECT --confirm--> LEVEL_SELECT --confirm--> PLAY
 *          ^                          ^    ^                  |
 *          +---------back-------------+    +--complete+confirm+
 *                                           (more levels: stays in PLAY
 *                                            and just loads the next one)
 */
#ifndef APP_H
#define APP_H

#include "input.h"
#include "carousel.h"
#include "character.h"
#include "levels.h"
#include "game.h"

typedef enum {
    SCENE_CHARACTER_SELECT,
    SCENE_LEVEL_SELECT,
    SCENE_PLAY
} SceneId;

typedef struct {
    SceneId       scene;
    int           fb_w, fb_h;

    CharacterList characters;   /* assets/characters.json */

    int           selected_character_id;
    int           selected_level_index;  /* index into g_levels[] */

    /* Working set for whichever select scene is active: for character
       select, one preview ModelInstance per entry (`previews`); for level
       select, one built-but-unused Level per entry (`level_defs`, just to
       own its `name` string - see enter_level_select). Either way, the
       CarouselItems in `items` point into whichever one is populated.
       Allocated on scene enter, freed on scene leave. */
    Carousel       carousel;
    ModelInstance *previews;
    Level         *level_defs;
    CarouselItem  *items;
    int            item_count;

    Game           game;         /* valid only while scene == SCENE_PLAY */
    int            game_active;
    int            prev_confirm; /* edge detection for leaving a completed level */
} App;

void app_init(App *a, int fb_w, int fb_h);
void app_step(App *a, const PlayerInput *in, float dt);
void app_render(App *a, int fb_w, int fb_h);
void app_shutdown(App *a);

#endif /* APP_H */
