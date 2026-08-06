# Desktop build (Windows / Linux / macOS) -- SDL2 + OpenGL fixed-function.
#
#   make            build ./kart (kart.exe on Windows)
#   make run        build and run
#   make clean      remove build artifacts
#
# Requires SDL2 dev libs and an sdl2-config on PATH.
#   - macOS/Linux: install via your package manager (brew/apt/dnf/...).
#   - Windows: build from an MSYS2 MinGW shell (not cmd.exe/PowerShell -
#     this Makefile needs a Unix-y shell for mkdir -p/rm -rf). Install the
#     toolchain + SDL2 with:
#       pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 make
#     then run `make` from the MinGW64 shell.
# GameCube build lives in Makefile.gc (devkitPro).
#
# Objects are built into build_pc/ (not alongside the sources in src/) so
# this build can never leave stale .o files that confuse the GameCube
# build's VPATH-based Makefile (which shares the same src/ directory).

CC      ?= cc
CSTD     = -std=c99
WARN     = -Wall -Wextra
OPT      = -O2
DEFS     = -DPLATFORM_PC
BUILD    = build_pc

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    GL_LIBS = -framework OpenGL
    BIN     = kart
else ifneq (,$(findstring MINGW,$(UNAME_S))$(findstring MSYS,$(UNAME_S)))
    # MSYS2/MinGW on Windows: the system GL library is opengl32, not GL.
    # sdl2-config --libs already pulls in whatever else the installed SDL2
    # package itself needs (it's built against the same MinGW system libs),
    # so no other Windows-specific libs need adding here.
    GL_LIBS = -lopengl32
    BIN     = kart.exe
else
    GL_LIBS = -lGL
    BIN     = kart
endif

CFLAGS  = $(CSTD) $(WARN) $(OPT) $(DEFS) $(SDL_CFLAGS) -Isrc
LIBS    = $(SDL_LIBS) $(GL_LIBS) -lm

SRC = \
    src/main.c \
    src/game.c \
    src/physics.c \
    src/track.c \
    src/circuit.c \
    src/race.c \
    src/hud.c \
    src/input.c \
    src/obj_loader.c \
    src/image_load.c \
    src/json.c \
    src/model_load.c \
    src/vehicle.c \
    src/character.c \
    src/carousel.c \
    src/app.c \
    src/render_gl.c

OBJ = $(patsubst src/%.c,$(BUILD)/%.o,$(SRC))

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LIBS)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(BUILD) $(BIN)

.PHONY: all run clean
