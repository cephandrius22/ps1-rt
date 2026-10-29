/*
 * episode.h — An ordered list of maps played one after another.
 *
 * Episodes are small text files so the order lives in one place:
 *
 *   # levels/episode1.txt
 *   title Descent
 *   map levels/e1m1.level Processing Plant
 *   map levels/e1m2.map   Pumping Station
 *
 * Paths are relative to where the game runs (the repository). A single
 * level run on its own (./rt levels/foo.map) is an episode of one map.
 */
#ifndef EPISODE_H
#define EPISODE_H

#include <stdbool.h>

#define EPISODE_MAPS_MAX  16
#define EPISODE_PATH_MAX  256
#define EPISODE_TITLE_MAX 48

typedef struct {
    char path[EPISODE_PATH_MAX];
    char title[EPISODE_TITLE_MAX];  /* "" if the file gives none */
} EpisodeMap;

typedef struct {
    char title[EPISODE_TITLE_MAX];
    EpisodeMap maps[EPISODE_MAPS_MAX];
    int count;
} Episode;

/* Read an episode file. False (with a message on stderr) if it can't be
 * read or lists no maps. */
bool episode_load(Episode *ep, const char *path);

/* An episode of just this level, titled after its file */
Episode episode_single(const char *level_path);

/* Is this a path to an episode file rather than a level? (".txt") */
bool episode_is_file(const char *path);

#endif
