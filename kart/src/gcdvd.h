/*
 * gcdvd.h - PLATFORM_GC only. Mounts the GameCube disc's own file layout
 * (the FST that Nintendo's disc-mastering tools write alongside main.dol)
 * as a read-only newlib device named "gcdvd:", so ordinary fopen()/fread()
 * calls against a path like "gcdvd:/assets/vehicles.json" transparently
 * read from the disc - the same relative path desktop resolves under its
 * working directory.
 *
 * This means the .dol is no longer self-contained: it must be run from a
 * disc (real or emulated/ISO) that also has the game's assets/ directory
 * staged next to it (i.e. under whatever your disc-authoring tool calls
 * "files/"), not booted standalone.
 */
#ifndef GCDVD_H
#define GCDVD_H

/* Reads the disc header + FST and registers the "gcdvd:" device. Safe to
 * call once at startup, before any asset loading. Returns 1 on success,
 * 0 if there's no disc, no FST, or the DVD hardware failed to init/mount -
 * callers should treat that as "no assets available" and fall back
 * gracefully (which vehicle loading already does), not as a fatal error.
 */
int gcdvd_mount(void);

#endif
