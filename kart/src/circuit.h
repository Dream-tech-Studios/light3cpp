/*
 * circuit.h - procedural oval track layout: barrier walls + ordered
 * checkpoint gates + a start pose.
 *
 * Deliberately decoupled from physics.c/Kart (like track.c is), so a
 * future real track - loaded from an OBJ authored in Blender, with
 * checkpoints placed by the track designer - is a drop-in replacement:
 * race.c only needs the Checkpoint list, and game.c only needs the Wall
 * list plus start_pos/start_heading, regardless of how the Circuit was
 * built.
 */
#ifndef CIRCUIT_H
#define CIRCUIT_H

#include "math3d.h"

/* A vertical barrier segment in the XZ plane (Y is ignored - walls block
   sideways movement no matter the kart's or wall's visual height). */
typedef struct {
    Vec3 a, b;
} Wall;

/*
 * A gate a racer must cross, in order, to make checkpoint/lap progress.
 * `forward` is the direction of legal travel through the gate; `half_width`
 * is how far off-center (from `center`, along the gate's perpendicular
 * axis) still counts as crossing it.
 */
typedef struct {
    Vec3  center;
    Vec3  forward;
    float half_width;
} Checkpoint;

typedef struct {
    Wall       *walls;
    int         wall_count;
    Checkpoint *checkpoints;
    int         checkpoint_count;  /* checkpoints[0] is the start/finish line */
    Vec3        start_pos;
    float       start_heading;     /* matches physics.c's heading convention */
} Circuit;

/*
 * Builds a simple rectangular oval: a `length` x `depth` loop, `track_width`
 * wide, with inner + outer barrier walls and one checkpoint at the middle
 * of each of the 4 straights.
 */
void circuit_make_oval(Circuit *c, float length, float depth, float track_width);
void circuit_free(Circuit *c);

/*
 * Keeps a circular collider of the given `radius` inside the barrier walls:
 * pushes `*pos` back out of any wall it has penetrated and cancels the
 * velocity component driving it further in (a simple slide-along-wall
 * response). Called once per frame, right after the kart's own physics
 * step - physics.c itself stays completely track-shape-agnostic.
 */
void circuit_resolve_walls(const Circuit *c, Vec3 *pos, Vec3 *vel, float radius);

#endif /* CIRCUIT_H */
