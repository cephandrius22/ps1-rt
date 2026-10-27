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
 *   frame dt ──> accumulator ──> game_tick() × N ──> entities, lights, AI,
 *   PlayerCmd ──> pending cmd ──┘                     player, weapon → damage
 *                                       └──> events (drained once per frame)
 *
 * Modes:
 *
 *   PLAYING ──player dies──────> DEAD ──────┐ attack/use after
 *      │                                    ├─ GAME_CONTINUE_DELAY ──>
 *      └──touches exit pad──> LEVEL_END ────┘ reload level (PLAYING)
 *
 * The world (lights, entities) keeps animating in every mode; enemies
 * freeze at LEVEL_END.
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
#define GAME_LEVEL_PATH_MAX      256
#define GAME_CONTINUE_DELAY      1.0f /* seconds in DEAD/LEVEL_END before input continues */

typedef enum {
    GAME_PLAYING,
    GAME_DEAD,      /* player input only respawns */
    GAME_LEVEL_END, /* exit reached; shows kills, input restarts */
} GameMode;

typedef enum {
    GAME_LOAD_OK,
    GAME_LOAD_FAILED,
} GameLoadResult;

typedef struct {
    GameMode mode;
    float mode_time;     /* seconds since mode last changed */
    char level_path[GAME_LEVEL_PATH_MAX];
    Scene scene;
    Player player;
    Weapon weapon;
    EventQueue events;   /* produced since the last game_update() began */
    float time;          /* seconds of simulated game time */
    float tick_accum;    /* frame time not yet simulated */
    int kills;           /* enemies killed this level */
    int kills_total;     /* enemies placed in the level */
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

/* Where the player is looking right now: view plus weapon recoil.
 * Used both to aim shots and to place the render camera. */
Pose game_view(const Game *g);

/* Ammo left for the current weapon (for the HUD) */
int game_ammo(const Game *g);

#endif
