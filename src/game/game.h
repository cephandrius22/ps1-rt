/*
 * game.h — Top-level game state: owns the world and everything in it.
 *
 * main.c only wires platform ↔ game ↔ render. All simulation goes
 * through this module, so level transitions, pause and save/load have
 * one struct to operate on.
 *
 *   PlayerCmd ──> game_tick() ──> entities, lights, player, weapon
 *                      │
 *                      └──> events (drained by main → audio)
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
    EventQueue events; /* produced since the last game_tick() began */
    float time;        /* seconds of simulated game time */
} Game;

/* Load a level and place the player at its spawn point */
GameLoadResult game_init(Game *g, const char *level_path);
void game_free(Game *g);

/* Advance the simulation by dt seconds */
void game_tick(Game *g, const PlayerCmd *cmd, float dt);

#endif
