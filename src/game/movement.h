/*
 * movement.h — Walking bodies: wall sliding, steps, ground under the feet.
 *
 * Shared by the player and enemies so both climb the same stairs and are
 * stopped by the same walls. A body is a vertical cylinder standing on
 * its feet:
 *
 *        ─┬─  height     wall probes start just above `step`, so anything
 *         │              lower is a step you walk onto (and the ground
 *    ────>│ probe        search lifts you onto it), not a wall you bump
 *    ────>│ probe
 *   ......│.... step     ┌──┐
 *        feet        ────┘  └──  step ≤ `step`: walk up
 */
#ifndef MOVEMENT_H
#define MOVEMENT_H

#include "scene.h"

#define MOVEMENT_NO_GROUND (-1000.0f)

typedef struct {
    float radius;
    float height;
    float step;     /* highest ledge walked up without jumping */
} BodyShape;

/* Walkable ground below a body: searched from `step` above the feet down
 * to max_drop below them, looking past steep surfaces. Returns its height
 * or MOVEMENT_NO_GROUND. */
float movement_ground(const Scene *s, Vec3 feet, float step, float max_drop);

/* Move the feet horizontally by move, X then Z so walls are slid along,
 * each axis stopping `radius` short of anything solid at body height */
Vec3 movement_slide(const Scene *s, Vec3 feet, Vec3 move, BodyShape shape);

#endif
