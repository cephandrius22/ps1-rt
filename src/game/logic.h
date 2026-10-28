/*
 * logic.h — Level logic: doors, keycards, buttons, triggers.
 *
 * World entities store a mover's shape and MoverState; this module
 * decides when it moves, like ai.c does for enemies:
 *
 *   CLOSED ──E / monster bumps / fired──> OPENING ──travel done──> OPEN
 *     ^                                                             │ wait
 *     └──────────── fully shut ──── CLOSING <───────────────────────┘
 *                                      │ body in the way (doors)
 *                                      └──> OPENING (Doom: doors never crush)
 *
 * Buttons use the same slide (pressed = open). Quake-style wiring:
 *
 *   trigger_once / trigger_multiple ──player enters──┐
 *   func_button ──────────────────────E──────────────┴──> fire `target`
 *        ──> every entity named it: door opens, hidden monster appears
 *        ──> every light named it: toggles on/off
 *        EV_TRIGGERED lets game.c show the source's `message`
 *
 * It also keeps the nav grid in step: a closed door stops sound, and a
 * locked one (keycard, or only opened by a trigger) stops paths too.
 */
#ifndef LOGIC_H
#define LOGIC_H

#include "scene.h"
#include "player.h"
#include "event.h"

/* Move doors and buttons one tick; reopen blocked doors; let chasing
 * monsters open unlocked doors they reach; fire triggers the player is
 * in. Call before the dynamic BVH rebuild. */
void logic_update(Scene *s, const Player *p, float dt, EventQueue *ev);

/* The player pressed use on what's in front: a door opens (or closes),
 * a button presses. Keyed ones need the player's keycard (else
 * EV_LOCKED); a named door ignores use, it waits for its trigger. */
void logic_use(Scene *s, const Player *p, EventQueue *ev);

/* Fire a name: everything with that targetname reacts (see above) */
void logic_fire(Scene *s, const char *name, EventQueue *ev);

#endif
