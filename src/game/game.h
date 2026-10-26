/*
 * game.h — Top-level game state: owns the world and everything in it.
 *
 * main.c only wires platform ↔ game ↔ render. All simulation goes
 * through this module, so level transitions, pause and save/load have
 * one struct to operate on.
 *
 * Simulation runs at a fixed GAME_TICK_HZ regardless of frame rate, so
 * physics and AI behave the same on every machine. game_update() turns
 * a variable frame time into 0..N fixed ticks:
 *
 *   frame dt ──> accumulator ──> game_tick() × N ──> entities, lights,
 *   PlayerCmd ──> pending cmd ──┘                     player, weapon
 *                                       └──> events (drained once per frame)
 *
 * Mode is the hook for menus, death and intermission; Phase 1 adds
 * modes as features need them.
 */
#ifndef GAME_H
#define GAME_H

#include "scene.h"
#include "player.h"
#include "weapon.h"
#include "event.h"
#include "cmd.h"

#define GAME_TICK_HZ             60
#define GAME_TICK_DT             (1.0f / GAME_TICK_HZ)
#define GAME_MAX_TICKS_PER_FRAME 5 /* beyond this, drop time (no death spiral) */

typedef enum {
    GAME_PLAYING,
} GameMode;

typedef enum {
    GAME_LOAD_OK,
    GAME_LOAD_FAILED,
} GameLoadResult;

typedef struct {
    GameMode mode;
    Scene scene;
    Player player;
    Weapon weapon;
    EventQueue events;   /* produced since the last game_update() began */
    float time;          /* seconds of simulated game time */
    float tick_accum;    /* frame time not yet simulated */
    PlayerCmd pending;   /* input waiting for the next tick */
} Game;

/* Load a level and place the player at its spawn point */
GameLoadResult game_init(Game *g, const char *level_path);
void game_free(Game *g);

/* Per frame: clear events, then run as many fixed ticks as frame_dt
 * covers. Returns the number of ticks run. */
int game_update(Game *g, const PlayerCmd *cmd, float frame_dt);

/* One fixed tick; appends to g->events. Exposed for tests. */
void game_tick(Game *g, const PlayerCmd *cmd);

#endif
