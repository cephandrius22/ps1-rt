/*
 * clock.h — High-resolution time for frame timing.
 */
#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

/* Seconds since an arbitrary start; only differences mean anything */
double clock_seconds(void);

/* A value that differs from run to run (seeds cosmetic randomness) */
uint32_t clock_seed(void);

#endif
