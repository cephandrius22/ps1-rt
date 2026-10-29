/*
 * bench.h — Measure frame and tick time on the shipped maps.
 *
 * Run as `./rt --bench [map ...]` (no maps: every shipped one). For each
 * map, renders fixed views on the thread pool, as the game does, and
 * runs game ticks with no input:
 *
 *   views   the spawn looking four ways, then each enemy's eye (yaw by
 *           index) — the same every run, so numbers compare across builds
 *   frame   per view, the median of its renders on the thread pool;
 *           the mean over views and the worst view
 *   1 core  the same views on one thread, fastest of two renders (not
 *           skewed by scheduling or efficiency cores: the steadiest
 *           number to compare builds by)
 *   tick    mean game_tick() time (AI, logic, dynamic BVH)
 *
 * Phase 7 measures every change with it. Like main.c, it may use any
 * layer.
 */
#ifndef BENCH_H
#define BENCH_H

#include <stdio.h>

/* Bench the given maps (count 0: the shipped ones); table to out.
 * Returns 0, or -1 if a map fails to load. */
int bench_run(const char *const maps[], int count, FILE *out);

#endif
