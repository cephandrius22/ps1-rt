/*
 * levelcheck.h — Warn about level mistakes when a level loads.
 *
 * Runs after every load and hot reload; prints one line per problem with
 * the position in meters and in map units (to find it in TrenchBroom):
 *
 *   check: ghoul #2 is 0.80 m above the floor (enemies don't fall)
 *          at (1.00, 0.80, -4.00) m, map (32 128 26)
 *
 * Checks: spawn and exit present; player spawn and enemies not inside or
 * overlapping static geometry; a floor under the spawn; enemies standing
 * on a floor; budget (triangles, lights, sprites). Unknown names and keys
 * are reported by the loaders themselves.
 *
 * Budgets come from measurements on the dev machine (Phase 1.5): frame
 * time grows ~0.12 ms per light and ~0.11 ms per sprite; triangles are
 * cheap thanks to the BVH.
 */
#ifndef LEVELCHECK_H
#define LEVELCHECK_H

#include "level.h"
#include <stdio.h>

#define BUDGET_TRIANGLES 30000
#define BUDGET_LIGHTS    32
#define BUDGET_SPRITES   40

/* Collision size of whoever spawns (the player): radius and height */
typedef struct {
    float radius;
    float height;
} BodySize;

/* Print warnings and a budget line to out. Returns the number of warnings. */
int level_check(const Scene *s, const LevelInfo *info, BodySize player, FILE *out);

#endif
