/*
 * nav.h — Walkable-space grid for enemy pathing and sound spreading.
 *
 * Built once at level load from static geometry; no designer work. The
 * level is cut into NAV_CELL squares; each square keeps up to
 * NAV_LAYERS walkable floors (so a platform above a hall works). A
 * floor counts only with NAV_HEADROOM of clear space above it.
 *
 *   top view, one layer:      side view, one cell:
 *   . . . # # . .               ── ceiling (faces down: ignored)
 *   . . . # # . .               ▔▔ platform  layer 1
 *   . . . . . . .   # = wall      (headroom)
 *                                ▔▔ floor     layer 0
 *
 * Neighbouring floors (8 directions) are linked when the step between
 * them is climbable, nothing solid stands in between, and there's no
 * hole. Diagonals also need both side links, so paths never cut
 * corners. A roof or a crate top never links down to the floor, so
 * nothing paths onto it.
 *
 * Floors near a wall cost more, keeping paths in the middle of halls.
 * Doors aren't static geometry, so doorways are open floor; at runtime
 * a closed door marks its nodes (nav_set_door): sound stops at any
 * closed door, paths only at locked ones (monsters open the others).
 */
#ifndef NAV_H
#define NAV_H

#include "bvh.h"
#include <stdint.h>

#define NAV_CELL      0.5f   /* meters */
#define NAV_LAYERS    4
#define NAV_HEADROOM  1.4f   /* clear height needed above a floor */
#define NAV_STEP      0.6f   /* highest step between neighbours (= walking bodies) */
#define NAV_NONE      (-1)

typedef enum {
    NAV_DOOR_OPENABLE, /* blocks sound; paths go through (monsters open it) */
    NAV_DOOR_LOCKED,   /* blocks sound and paths (keycard, trigger-only) */
} NavDoor;

typedef struct {
    float f;    /* cost so far + estimate to the goal */
    int node;
} NavHeapItem;

typedef struct {
    float min_x, min_z;     /* world position of cell (0, 0)'s corner */
    int nx, nz;             /* cells along X and Z */
    int node_count;         /* nx * nz * NAV_LAYERS (most slots empty) */

    /* Per node; index = (cz * nx + cx) * NAV_LAYERS + layer */
    float *height;          /* floor height; NAN = no floor */
    uint8_t *links;         /* bit d = can walk to the neighbour in direction d */
    uint8_t *tight;         /* 1 = a wall is close (costs more) */
    uint8_t *closed;        /* closed doors covering the node (sound stops) */
    uint8_t *locked;        /* closed locked doors covering it (paths stop) */

    /* Search scratch, reused per query: a node's entry is valid only when
     * its stamp equals the current search number (no clearing) */
    uint32_t *seen;
    uint32_t *done;
    uint32_t search;
    float *cost;
    int *parent;
    NavHeapItem *heap;
    int heap_cap;
    int heap_count;

    /* Last sound spread: heard where flood_seen == flood */
    uint32_t *flood_seen;
    uint32_t flood;
} NavGrid;

/* Build from the static BVH (call after scene_build_static). Replaces
 * any previous grid. An empty BVH gives an empty grid. */
void nav_build(NavGrid *nav, const BVH *world);
void nav_free(NavGrid *nav);

/* Nodes that have a floor (for logging and budgets) */
int nav_floor_count(const NavGrid *nav);

/* The floor node a body standing at feet is on, or NAV_NONE */
int nav_node_at(const NavGrid *nav, Vec3 feet);

/* World position of a node: cell center, on the floor */
Vec3 nav_node_pos(const NavGrid *nav, int node);

/* A* from one position to another. On success writes where to steer
 * next (the end of the path's first straight run) and returns true. */
bool nav_next_waypoint(NavGrid *nav, Vec3 from, Vec3 to, Vec3 *waypoint);

/* Spread a noise from `from` through open space, up to max_dist meters
 * of walking distance; closed doors stop it. Query with nav_heard(). */
void nav_flood(NavGrid *nav, Vec3 from, float max_dist);
bool nav_heard(const NavGrid *nav, Vec3 feet);

/* Add (+1) or remove (-1) a closed door over the nodes near the world
 * box [lo, hi]. The box is widened by half a cell so even a thin door
 * covers a row of cell centers. */
void nav_set_door(NavGrid *nav, Vec3 lo, Vec3 hi, NavDoor kind, int delta);

#endif
