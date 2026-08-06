/*
 * input.c - SDL2 input backend (PLATFORM_PC).
 *
 * Keyboard:
 *   W / Up      move forward
 *   S / Down    move backward
 *   A / Left    turn left
 *   D / Right   turn right
 *   Space       jump
 *   Esc         quit
 * Also reads the first connected game controller if present.
 */
#ifdef PLATFORM_PC

#include <SDL.h>
#include "input.h"

static SDL_GameController *g_pad = NULL;

void input_init(void) {
    int i;
    for (i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            g_pad = SDL_GameControllerOpen(i);
            if (g_pad) break;
        }
    }
}

void input_shutdown(void) {
    if (g_pad) {
        SDL_GameControllerClose(g_pad);
        g_pad = NULL;
    }
}

void input_poll(PlayerInput *out) {
    const Uint8 *keys;
    SDL_Event ev;

    out->move = 0.0f;
    out->turn = 0.0f;
    out->jump = 0;
    out->menu_left = out->menu_right = out->menu_confirm = out->menu_back = 0;
    /* out->quit is sticky once set */

    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) out->quit = 1;
        if (ev.type == SDL_CONTROLLERDEVICEADDED && !g_pad)
            g_pad = SDL_GameControllerOpen(ev.cdevice.which);
    }

    keys = SDL_GetKeyboardState(NULL);
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])    out->move += 1.0f;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])  out->move -= 1.0f;
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])  out->turn -= 1.0f;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) out->turn += 1.0f;
    if (keys[SDL_SCANCODE_SPACE])                         out->jump  = 1;
    if (keys[SDL_SCANCODE_ESCAPE])                        out->quit  = 1;

    /* Menu navigation (arrows or A/D to move, Enter/Space to confirm,
       Esc/Backspace to go back). */
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])   out->menu_left    = 1;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])  out->menu_right   = 1;
    if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER] ||
        keys[SDL_SCANCODE_SPACE])                          out->menu_confirm = 1;
    /* Backspace (not Esc) for "back": Esc is the global quit key above, so
       reusing it would exit the whole game instead of stepping back a scene. */
    if (keys[SDL_SCANCODE_BACKSPACE])                      out->menu_back = 1;

    if (g_pad) {
        /* Left stick Y = move forward/back, left stick X = turn */
        float ly = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
        float lx = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;

        if (ly < -0.15f || ly > 0.15f) out->move -= ly; /* stick Y is down-positive */
        if (lx < -0.15f || lx > 0.15f) out->turn += lx;

        if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_A) ||
            SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
            out->jump = 1;

        /* Menu nav: d-pad or left stick to move, A to confirm, B to back. */
        if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)  || lx < -0.5f)
            out->menu_left = 1;
        if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || lx >  0.5f)
            out->menu_right = 1;
        if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_A))
            out->menu_confirm = 1;
        if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_B))
            out->menu_back = 1;
    }

    if (out->move < -1.0f) out->move = -1.0f;
    if (out->move >  1.0f) out->move =  1.0f;
    if (out->turn < -1.0f) out->turn = -1.0f;
    if (out->turn >  1.0f) out->turn =  1.0f;
}

#endif /* PLATFORM_PC */

/*
 * GameCube backend (PLATFORM_GC): reads controller port 0 via libogc's PAD
 * API.
 *
 *   Control stick   move (Y) / turn (X)
 *   A button        jump
 *
 * There is no windowing system to "quit" on a console, so `quit` is never
 * set here; the game just runs until the machine is powered off or reset.
 */
#ifdef PLATFORM_GC

#include <gccore.h>
#include "input.h"

#define STICK_RANGE  100.0f  /* GC control stick reports roughly +/-100 */
#define STICK_DEADZONE 8.0f

void input_init(void) {
    PAD_Init();
}

void input_shutdown(void) {
}

void input_poll(PlayerInput *out) {
    s8  sx, sy;
    u16 held;

    PAD_ScanPads();

    sx   = PAD_StickX(0);
    sy   = PAD_StickY(0);
    held = PAD_ButtonsHeld(0);

    out->jump = (held & PAD_BUTTON_A) ? 1 : 0;

    out->move = 0.0f;
    if (sy > STICK_DEADZONE || sy < -STICK_DEADZONE)
        out->move = (float)sy / STICK_RANGE;

    out->turn = 0.0f;
    if (sx > STICK_DEADZONE || sx < -STICK_DEADZONE)
        out->turn = (float)sx / STICK_RANGE;

    if (out->move < -1.0f) out->move = -1.0f;
    if (out->move >  1.0f) out->move =  1.0f;
    if (out->turn < -1.0f) out->turn = -1.0f;
    if (out->turn >  1.0f) out->turn =  1.0f;

    /* Menu nav: d-pad or control stick to move, A to confirm, B to back. */
    out->menu_left    = (held & PAD_BUTTON_LEFT)  || sx < -STICK_RANGE * 0.5f;
    out->menu_right   = (held & PAD_BUTTON_RIGHT) || sx >  STICK_RANGE * 0.5f;
    out->menu_confirm = (held & PAD_BUTTON_A) ? 1 : 0;
    out->menu_back    = (held & PAD_BUTTON_B) ? 1 : 0;
}

#endif /* PLATFORM_GC */
