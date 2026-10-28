/*
 * logic.h — Level logic: doors, keycards (and, next, buttons, triggers).
 *
 * World entities store a door's shape and MoverState; this module decides
 * when it moves, like ai.c does for enemies:
 *
 *   CLOSED ──E / monster bumps──> OPENING ──travel done──> OPEN
 *     ^                                                     │ wait
 *     └──────── fully shut ──── CLOSING <───────────────────┘
 *                                  │ body in the way
 *                                  └──> OPENING (Doom: doors never crush)
 *
 * It also keeps the nav grid in step: a closed door stops sound, and a
 * locked one (keycard, or only opened by a trigger) stops paths too.
 */
#ifndef LOGIC_H
#define LOGIC_H

#include "scene.h"
#include "player.h"
#include "event.h"

/* Move doors one tick; reopen blocked ones; let chasing monsters open
 * unlocked doors they reach. Call before the dynamic BVH rebuild. */
void logic_update(Scene *s, const Player *p, float dt, EventQueue *ev);

/* The player pressed use: open (or close) the door in front. A keyed
 * door needs the player's keycard (else EV_DOOR_LOCKED); a named door
 * ignores use, it waits for its trigger. */
void logic_use(Scene *s, const Player *p, EventQueue *ev);

#endif
