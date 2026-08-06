/*
 * input.h - platform-agnostic input state for the kart.
 *
 * The game only ever sees a KartInput. Each platform fills it in:
 *   input.c (PLATFORM_PC) -> SDL2 keyboard + game controller
 *   input.c (PLATFORM_GC) -> libogc PAD          [future]
 */
#ifndef INPUT_H
#define INPUT_H

typedef struct {
    float throttle;  /* 0..1  forward */
    float brake;     /* 0..1  reverse / brake */
    float steer;     /* -1..1 (left .. right) */
    int   drift;     /* held drift button (boolean) */
    int   quit;      /* user requested to quit */

    /* Menu navigation, for the character/vehicle select carousels. These are
       reported as raw held state each frame; the menu code does its own
       press-vs-hold edge detection so one press = one step. */
    int   menu_left;
    int   menu_right;
    int   menu_confirm;
    int   menu_back;
} KartInput;

void input_init(void);
void input_shutdown(void);

/* Pump platform events and refresh `out`. Call once per frame. */
void input_poll(KartInput *out);

#endif /* INPUT_H */
