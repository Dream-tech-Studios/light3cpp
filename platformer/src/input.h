/*
 * input.h - platform-agnostic input state for the platforming character.
 *
 * The game only ever sees a PlayerInput. Each platform fills it in:
 *   input.c (PLATFORM_PC) -> SDL2 keyboard + game controller
 *   input.c (PLATFORM_GC) -> libogc PAD
 */
#ifndef INPUT_H
#define INPUT_H

typedef struct {
    float move;   /* -1..1  backward .. forward, relative to facing */
    float turn;   /* -1..1  turn left .. turn right */
    int   jump;   /* held state of the jump button; player.c edge-detects it */
    int   quit;   /* user requested to quit */

    /* Menu navigation, for the character select carousel. These are
       reported as raw held state each frame; the menu code does its own
       press-vs-hold edge detection so one press = one step. */
    int   menu_left;
    int   menu_right;
    int   menu_confirm;
    int   menu_back;
} PlayerInput;

void input_init(void);
void input_shutdown(void);

/* Pump platform events and refresh `out`. Call once per frame. */
void input_poll(PlayerInput *out);

#endif /* INPUT_H */
