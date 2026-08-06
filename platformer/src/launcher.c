#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>

// Handle Windows lack of POSIX strcasecmp
#if defined(_WIN32) || defined(_WIN64)
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif

#define SCREEN_W 1280
#define SCREEN_H 720

// ---------------------------------------------------------------------------
// Game entry types
// ---------------------------------------------------------------------------

typedef enum {
    LAUNCH_FILE,        // Launch an executable/script
    LAUNCH_TITLE        // Mock launch a native app/title
} LaunchType;

typedef struct {
    const char *title;
    const char *subtitle;
    const char *thumb_path;
    const char *bg_path;
    LaunchType  launch_type;
    const char *exec_path;
    uint64_t    title_id;
} GameEntry;

#define NUM_GAMES 4

static GameEntry GAMES[NUM_GAMES] = {
    {
        "Game",
        "Desc",
        "images/game1_thumb.png",
        "images/game1_bg.png",
        LAUNCH_FILE,
        "games/game1_executable",
        0
    },
    {
        "Game",
        "Desc",
        "images/game1_thumb.png",
        "images/game1_bg.png",
        LAUNCH_FILE,
        "games/game1_executable",
        0
    },
    {
        "Game",
        "Desc",
        "images/game1_thumb.png",
        "images/game1_bg.png",
        LAUNCH_FILE,
        "games/game1_executable",
        0
    },
    {
        "Game",
        "Desc",
        "images/game1_thumb.png",
        "images/game1_bg.png",
        LAUNCH_FILE,
        "games/game1_executable",
        0
    },
};

static const char *START_THUMB_PATH = "images/start_thumb.png";
static const char *START_BG_PATH    = "images/start_bg.png";

// ---------------------------------------------------------------------------
// Music playlist
// ---------------------------------------------------------------------------

#define MUSIC_DIR   "music/"
#define MAX_TRACKS  64

static char  s_tracks[MAX_TRACKS][512];
static int   s_trackCount = 0;
static int   s_trackIndex = 0;
static Mix_Music *s_music = NULL;

static void musicScanDir(void) {
    DIR *dir = opendir(MUSIC_DIR);
    if (!dir) {
        printf("Warn: Could not open music directory '%s'\n", MUSIC_DIR);
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && s_trackCount < MAX_TRACKS) {
        const char *name = ent->d_name;
        size_t len = strlen(name);
        if (len > 4 &&
            (strcasecmp(name + len - 4, ".mp3") == 0 ||
             strcasecmp(name + len - 4, ".ogg") == 0)) {
            snprintf(s_tracks[s_trackCount], sizeof(s_tracks[0]),
                     "%s%s", MUSIC_DIR, name);
            s_trackCount++;
        }
    }
    closedir(dir);
}

static void musicPlayNext(void) {
    if (s_trackCount == 0) return;
    if (s_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(s_music);
        s_music = NULL;
    }
    s_music = Mix_LoadMUS(s_tracks[s_trackIndex]);
    if (s_music) {
        Mix_PlayMusic(s_music, 1);
    }
    s_trackIndex = (s_trackIndex + 1) % s_trackCount;
}

static void onMusicFinished(void) {
    musicPlayNext();
}

// ---------------------------------------------------------------------------
// Rendering helpers
// ---------------------------------------------------------------------------

static SDL_Texture *loadTexture(SDL_Renderer *renderer, const char *path) {
    SDL_Texture *tex = IMG_LoadTexture(renderer, path);
    if (!tex) {
        printf("Warn: could not load '%s': %s\n", path, IMG_GetError());
    }
    return tex;
}

static void drawFullscreen(SDL_Renderer *renderer, SDL_Texture *tex) {
    if (!tex) return;
    SDL_Rect dst = { 0, 0, SCREEN_W, SCREEN_H };
    SDL_RenderCopy(renderer, tex, NULL, &dst);
}

static void drawBottomBar(SDL_Renderer *renderer) {
    for (int y = 580; y < SCREEN_H; y++) {
        int alpha = (int)(180.0f * (y - 580) / (SCREEN_H - 580));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)alpha);
        SDL_Rect line = { 0, y, SCREEN_W, 1 };
        SDL_RenderFillRect(renderer, &line);
    }
}

static void drawStripTile(SDL_Renderer *renderer, SDL_Texture *tex,
                           int x, int y, int w, int h, bool selected) {
    int rx = x, ry = y, rw = w, rh = h;

    if (selected) {
        int grow = (int)(w * 0.08f);
        rw = w + grow;
        rh = h + grow;
        rx = x - grow / 2;
        ry = y - grow - 4;
    }

    SDL_Rect dst = { rx, ry, rw, rh };

    if (tex) {
        SDL_RenderCopy(renderer, tex, NULL, &dst);
    } else {
        SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
        SDL_RenderFillRect(renderer, &dst);
    }

    if (selected) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (int i = 0; i < 3; i++) {
            SDL_Rect b = { dst.x - i, dst.y - i,
                           dst.w + i * 2, dst.h + i * 2 };
            SDL_RenderDrawRect(renderer, &b);
        }
    } else {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 90);
        SDL_RenderFillRect(renderer, &dst);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
}

static void launchGame(GameEntry *game) {
    printf("==========================================\n");
    printf("Launching: %s\n", game->title);
    
    if (game->launch_type == LAUNCH_FILE && game->exec_path) {
        printf("Executing file: %s\n", game->exec_path);
        // Uncomment to actually launch standard desktop binaries/scripts:
        // system(game->exec_path); 
    } else {
        printf("Mock launching Title ID: 0x%016llx\n", (unsigned long long)game->title_id);
    }
    printf("==========================================\n");
}

static void cleanupSDL(SDL_Renderer *renderer, SDL_Window *window,
                        SDL_Texture **thumbs, SDL_Texture **bgs,
                        SDL_Texture *startThumb, SDL_Texture *startBg) {
    if (s_music) { Mix_HaltMusic(); Mix_FreeMusic(s_music); s_music = NULL; }
    Mix_CloseAudio();
    Mix_Quit();

    for (int i = 0; i < NUM_GAMES; i++) {
        if (thumbs[i]) SDL_DestroyTexture(thumbs[i]);
        if (bgs[i])    SDL_DestroyTexture(bgs[i]);
    }
    if (startThumb) SDL_DestroyTexture(startThumb);
    if (startBg)    SDL_DestroyTexture(startBg);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
}

// ---------------------------------------------------------------------------
// App screens
// ---------------------------------------------------------------------------

typedef enum {
    SCREEN_SPLASH,
    SCREEN_SELECT
} AppScreen;

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }
    
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG))) {
        printf("IMG_Init failed: %s\n", IMG_GetError());
    }
    
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
        printf("Mix_OpenAudio failed: %s\n", Mix_GetError());
    } else {
        Mix_HookMusicFinished(onMusicFinished);
        musicScanDir();
        musicPlayNext();
    }

    SDL_Window *window = SDL_CreateWindow(
        "Mario Kart Ultimate Selector",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
        
    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit(); return -1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        
    if (!renderer) {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window); SDL_Quit(); return -1;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // ---- Load art ----
    SDL_Texture *startThumb = loadTexture(renderer, START_THUMB_PATH);
    SDL_Texture *startBg    = loadTexture(renderer, START_BG_PATH);

    SDL_Texture *gameThumbs[NUM_GAMES];
    SDL_Texture *gameBgs[NUM_GAMES];
    for (int i = 0; i < NUM_GAMES; i++) {
        gameThumbs[i] = loadTexture(renderer, GAMES[i].thumb_path);
        gameBgs[i]    = loadTexture(renderer, GAMES[i].bg_path);
    }

    const int tileW  = 200;
    const int tileH  = 200;
    const int gap    = 24;
    const int rowW   = NUM_GAMES * tileW + (NUM_GAMES - 1) * gap;
    const int rowX0  = (SCREEN_W - rowW) / 2;
    const int rowY   = SCREEN_H - tileH - 28;

    AppScreen screen = SCREEN_SPLASH;
    int selected = 0;
    bool running = true;

    while (running) {
        bool btn_A = false, btn_B = false, btn_Left = false, btn_Right = false, btn_Plus = false;
        SDL_Event e;
        
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_RETURN: btn_A = true; break;       // Enter acts as A
                    case SDLK_BACKSPACE: btn_B = true; break;    // Backspace acts as B
                    case SDLK_ESCAPE: btn_Plus = true; break;    // Escape acts as + (quit)
                    case SDLK_LEFT: btn_Left = true; break;
                    case SDLK_RIGHT: btn_Right = true; break;
                }
            }
        }

        if (screen == SCREEN_SPLASH) {
            if (btn_A) {
                screen = SCREEN_SELECT;
            }
            if (btn_Plus) {
                running = false;
            }
        } else {
            if (btn_Left) {
                selected = (selected - 1 + NUM_GAMES) % NUM_GAMES;
            }
            if (btn_Right) {
                selected = (selected + 1) % NUM_GAMES;
            }
            if (btn_B) {
                screen = SCREEN_SPLASH;
            }
            if (btn_A) {
                launchGame(&GAMES[selected]);
                running = false;
            }
        }

        SDL_SetRenderDrawColor(renderer, 10, 10, 15, 255);
        SDL_RenderClear(renderer);

        if (screen == SCREEN_SPLASH) {
            drawFullscreen(renderer, startBg);

            int sw = 420, sh = 420;
            int sx = (SCREEN_W - sw) / 2;
            int sy = (SCREEN_H - sh) / 2 - 30;
            SDL_Rect dst = { sx, sy, sw, sh };
            if (startThumb) {
                SDL_RenderCopy(renderer, startThumb, NULL, &dst);
            }

        } else {
            drawFullscreen(renderer, gameBgs[selected]);
            drawBottomBar(renderer);

            for (int i = 0; i < NUM_GAMES; i++) {
                int x = rowX0 + i * (tileW + gap);
                drawStripTile(renderer, gameThumbs[i],
                              x, rowY, tileW, tileH, i == selected);
            }
        }

        SDL_RenderPresent(renderer);
    }

    cleanupSDL(renderer, window, gameThumbs, gameBgs, startThumb, startBg);
    return 0;
}