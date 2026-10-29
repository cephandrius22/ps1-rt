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
 * Modes (continue = attack/use after GAME_CONTINUE_DELAY):
 *
 *   PLAYING ──player dies──> DEAD ──continue──> same map, entry loadout
 *      │
 *      └──exit pad──> LEVEL_END ──continue──> next map, carrying loadout
 *                         │ (last map)
 *                         └──continue──> EPISODE_END ──continue──> map 1, fresh
 *
 * Doom rules between maps: health, ammo and weapons carry over; keycards
 * don't. Dying restarts the map with what you entered it with.
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
#include "episode.h"

#define GAME_TICK_HZ             60
#define GAME_TICK_DT             (1.0f / GAME_TICK_HZ)
#define GAME_MAX_TICKS_PER_FRAME 5 /* beyond this, drop time (no death spiral) */
#define GAME_LEVEL_PATH_MAX      256
#define GAME_CONTINUE_DELAY      1.0f /* seconds in DEAD/LEVEL_END before input continues */
#define GAME_MESSAGE_MAX         64
#define GAME_MESSAGE_TIME        3.0f /* seconds a notice stays on screen */

typedef enum {
    GAME_PLAYING,
    GAME_DEAD,      /* player input only respawns */
    GAME_LEVEL_END, /* exit reached; shows kills, input goes to the next map */
    GAME_EPISODE_END, /* last map done; input starts the episode over */
} GameMode;

/* What the player carries into a map */
typedef struct {
    float health;
    Inventory inv;      /* keycards cleared */
    WeaponType weapon;  /* held when entering */
} Loadout;

typedef enum {
    GAME_LOAD_OK,
    GAME_LOAD_FAILED,
} GameLoadResult;

typedef struct {
    GameMode mode;
    float mode_time;     /* seconds since mode last changed */
    char level_path[GAME_LEVEL_PATH_MAX]; /* the current map's file */
    Episode episode;
    int map_index;       /* into episode.maps */
    Loadout entry;       /* what the player entered this map with */
    Scene scene;
    Player player;
    Weapon weapon;
    EventQueue events;   /* produced since the last game_update() began */
    float time;          /* seconds of simulated game time */
    float tick_accum;    /* frame time not yet simulated */
    int kills;           /* enemies killed this level */
    int kills_total;     /* enemies placed in the level */
    int secrets;         /* trigger_secret areas found this level */
    int secrets_total;
    float map_time;      /* seconds spent playing this map (intermission) */
    PlayerCmd pending;   /* input waiting for the next tick */
    char message[GAME_MESSAGE_MAX]; /* latest notice, e.g. "You need the red keycard" */
    float message_time;  /* seconds it has left on screen */
} Game;

/* Start an episode at its first map with the starting loadout */
GameLoadResult game_start(Game *g, const Episode *episode);

/* Play one level on its own (an episode of one map) */
GameLoadResult game_init(Game *g, const char *level_path);

/* Replace the running map with map `index` of g->episode, the player
 * carrying `loadout` (episode flow, loading a save) */
GameLoadResult game_enter_map(Game *g, int index, Loadout loadout);

/* Show a notice on screen, like "Game saved" */
void game_notify(Game *g, const char *text);
void game_free(Game *g);

/* Per frame: clear events, then run as many fixed ticks as frame_dt
 * covers. Returns the number of ticks run. */
int game_update(Game *g, const PlayerCmd *cmd, float frame_dt);

/* Reload the level file in place for editing: world and enemies reset,
 * the player keeps position and view. On failure the current level keeps
 * running. Mode returns to PLAYING. */
GameLoadResult game_reload(Game *g);

/* One fixed tick; appends to g->events. Exposed for tests. */
void game_tick(Game *g, const PlayerCmd *cmd);

/* Where the player is looking right now: view plus weapon recoil.
 * Used both to aim shots and to place the render camera. */
Pose game_view(const Game *g);

/* Ammo left for the current weapon (for the HUD) */
int game_ammo(const Game *g);

/* The notice to show now, or NULL. Written from this tick's events, so
 * gameplay modules push events and never format text. */
const char *game_message(const Game *g);

#endif
