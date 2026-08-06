/*
 * main.c - desktop platform layer (PLATFORM_PC).
 *
 * Owns the SDL2 window + OpenGL context and the timing loop. Requests a
 * default (compatibility) GL context so the fixed-function calls in
 * render_gl.c work on Windows, Linux and macOS without a function loader.
 *
 * Uses a fixed-timestep accumulator: physics always advances in 1/60s
 * steps regardless of display rate, which keeps the driving feel stable.
 */
#ifdef PLATFORM_PC

#include <SDL.h>
#include "app.h"
#include "input.h"
#include "render.h"

#if defined(_WIN32)
  #include <direct.h>
  #define CHDIR _chdir
#else
  #include <unistd.h>
  #define CHDIR chdir
#endif

#define WIN_W 1280
#define WIN_H 720
#define FIXED_DT (1.0f / 60.0f)

/*
 * Assets (bike.obj, etc.) are loaded via plain relative paths, which only
 * resolve correctly if the process's current working directory happens to
 * be the project folder. That's true when launched via `make run` from a
 * shell in that folder, but not in general (double-click, a shortcut, a
 * different shell cwd, ...). Fix it once at startup by chdir-ing into the
 * directory the executable itself lives in, so relative asset paths work
 * no matter how the binary is launched.
 */
static void chdir_to_executable_dir(void) {
    char *base = SDL_GetBasePath();
    if (base) {
        CHDIR(base);
        SDL_free(base);
    }
}

int main(int argc, char **argv) {
    SDL_Window   *win;
    SDL_GLContext ctx;
    App   app;
    KartInput in;
    Uint64 prev, now, freq;
    float  accumulator = 0.0f;

    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    chdir_to_executable_dir();

    /* Default profile = compatibility, which keeps immediate mode working.
       Ask for a depth buffer and double buffering. */
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    win = SDL_CreateWindow("C Kart",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           WIN_W, WIN_H,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!win) {
        SDL_Log("CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        SDL_Log("GL context failed: %s", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1); /* vsync */

    input_init();
    app_init(&app, WIN_W, WIN_H);
    render_set_viewport(WIN_W, WIN_H);

    in.quit = 0;
    freq = SDL_GetPerformanceFrequency();
    prev = SDL_GetPerformanceCounter();

    while (!in.quit) {
        float frame_dt;
        int win_w = WIN_W, win_h = WIN_H;

        now = SDL_GetPerformanceCounter();
        frame_dt = (float)((double)(now - prev) / (double)freq);
        prev = now;
        if (frame_dt > 0.25f) frame_dt = 0.25f; /* avoid spiral of death */

        input_poll(&in);

        /* Handle window resize each frame (cheap). */
        SDL_GL_GetDrawableSize(win, &win_w, &win_h);
        render_set_viewport(win_w, win_h);

        accumulator += frame_dt;
        while (accumulator >= FIXED_DT) {
            app_step(&app, &in, FIXED_DT);
            accumulator -= FIXED_DT;
        }

        app_render(&app, win_w, win_h);
        SDL_GL_SwapWindow(win);
    }

    app_shutdown(&app);
    input_shutdown();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

#endif /* PLATFORM_PC */
