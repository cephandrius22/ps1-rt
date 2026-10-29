/*
 * save.h — Save the whole game anywhere; load it back.
 *
 * A save holds everything that changes during play; the map's geometry
 * is reloaded from its file. Layout (binary, this build only):
 *
 *   header   magic, version, sizes of the saved structs
 *   info     episode title, map title, time on map (menus read just this)
 *   game     episode, map index, entry loadout, player, weapon, tallies
 *   scene    every entity (doors, monsters, pickups), every light
 *
 * Loading reloads the map, checks its entities still match the save's
 * (else the level was edited: the map starts fresh instead), then puts
 * the saved state back. Doors re-mark the nav grid on the next tick.
 *
 * Structs are written as they are in memory, so a save only loads into
 * the build that wrote it; the header's sizes catch other builds.
 */
#ifndef SAVE_H
#define SAVE_H

#include "game.h"

#define SAVE_DIR        "saves"
#define SAVE_QUICK      SAVE_DIR "/quick.sav"
#define SAVE_SLOTS      5          /* saves/slot1.sav .. slot5.sav */
#define SAVE_PATH_MAX   64

/* What a menu shows for a save without loading it */
typedef struct {
    char episode_title[EPISODE_TITLE_MAX];
    char map_title[EPISODE_TITLE_MAX];
    int map_index;
    float map_time;   /* seconds on the map when saved */
} SaveInfo;

/* "saves/slot<n>.sav" for slot 1..SAVE_SLOTS */
void save_slot_path(int slot, char *out, int size);

/* Write the running game (the folder must exist). False on failure or
 * outside GAME_PLAYING. */
bool game_save(const Game *g, const char *path);

/* Replace the running game with a save. GAME_LOAD_FAILED leaves the
 * game untouched if the file is missing, damaged or from another build. */
GameLoadResult game_load(Game *g, const char *path);

/* Read just the info block. False if there's no valid save at path. */
bool save_peek(const char *path, SaveInfo *info);

#endif
