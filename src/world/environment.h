/*
 * environment.h — Level-wide light and air: sun, ambient, fog, sky.
 *
 * Each level sets its own mood; anything it leaves out keeps the default
 * (the dark, brown-fogged plant of e1m1). Same keywords in both formats:
 *
 *   .level line                  .map worldspawn key (lengths in units,
 *                                sun direction in map axes; the loader
 *                                converts before calling environment_set)
 *   ambient 0.04                 "ambient" "0.04"
 *   fog 8 22 0.04 0.03 0.02      "fog" "256 704 0.04 0.03 0.02"
 *   sun 0.5 1 -0.3 1 .95 .7 .15  "sun" "..."   direction toward the sun,
 *                                              color, intensity (0 = none)
 *   sky 0.02 0.01 0.04 0.08 0.06 0.03          top color, horizon color
 *   reverb 0.15                  "reverb" "0.15"  echo: 0 dry .. 1 cavernous
 *   ambience drone               "ambience" "drone"  background loop by name
 *                                (drone, drip, hum, wind, organ, bell,
 *                                none; main.c plays it)
 *
 *   fog: start ──────── end
 *        clear  blends   fully fog_color
 *
 * Zones override parts of it inside a box, like Doom sectors:
 *
 *   .level: zone x1 y1 z1 x2 y2 z2 ambient=0.01 fog="3 12 0.03 0.05 0.02"
 *                                  reverb=0.6 ambience=drip
 *   .map:   env_zone brush entity with the same keys (fog in units)
 *
 *   ┌───────────┐┌──────┐   ambient: per surface, from the zone the
 *   │ hall      ││ crypt│   surface is in (a dark room stays dark seen
 *   │ level env ││ zone │   from a bright one); fog and sound: from the
 *   └───────────┘└──────┘   zone the camera is in, blended by the caller
 */
#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "math3d.h"
#include <stdbool.h>

#define ENV_NAME_MAX 16

typedef struct {
    float ambient;        /* light everywhere, before sun and lamps */

    Vec3 sun_dir;         /* toward the sun, normalized */
    Vec3 sun_color;
    float sun_intensity;  /* 0 = no sun (and no sun shadow rays) */

    Vec3 fog_color;
    float fog_start;      /* meters: fog begins */
    float fog_end;        /* meters: fully fogged */

    Vec3 sky_top;         /* rays that hit nothing: gradient up ... */
    Vec3 sky_horizon;     /* ... to the horizon and below */

    float reverb;         /* echo on every sound: 0 dry .. 1 cavernous */
    char ambience[ENV_NAME_MAX]; /* background loop by name ("none" = silence) */
} Environment;

/* What a zone overrides */
typedef enum {
    ZONE_SETS_AMBIENT = 1 << 0,
    ZONE_SETS_FOG     = 1 << 1,
    ZONE_SETS_SOUND   = 1 << 2, /* reverb and/or ambience */
} ZoneSets;

typedef struct {
    Vec3 lo, hi;       /* world box */
    Environment env;   /* the level's, with this zone's keys applied */
    unsigned sets;     /* ZoneSets bits */
} EnvZone;

typedef struct {
    EnvZone *items;
    int count;
    int capacity;
} ZoneList;

/* The default mood (what levels without environment lines get) */
Environment environment_default(void);

/* Apply one keyword with its numbers, e.g. ("fog", "8 22 0.04 0.03 0.02").
 * Engine units (meters, Y up). False for an unknown keyword or too few
 * numbers; the environment is unchanged then. */
bool environment_set(Environment *env, const char *key, const char *value);

/* Fog blend for a hit at this distance: 0 = clear, 1 = all fog */
float environment_fog(const Environment *env, float distance);

/* Background color for a ray that hit nothing */
Vec3 environment_sky(const Environment *env, Vec3 dir);

/* Fade fog, ambient and reverb from a toward b by t (0..1); the rest
 * (sun, sky, ambience name) is b's */
Environment environment_blend(const Environment *a, const Environment *b, float t);

/* Growable zone array; zone_add returns false if out of memory */
void zone_list_init(ZoneList *zones);
void zone_list_free(ZoneList *zones);
bool zone_add(ZoneList *zones, EnvZone zone);

/* A zone over [lo, hi] from ambient=, fog="start end r g b", reverb= and
 * ambience= keys (engine units) applied on top of base. False if none
 * is set. */
bool zone_from_keys(EnvZone *zone, Vec3 lo, Vec3 hi, const Environment *base,
                    const char *args);

/* Ambient light for a surface at p: its zone's, else the level's */
float environment_ambient_at(const Environment *base, const ZoneList *zones, Vec3 p);

/* What an eye (or ear) at p gets: its zones' fog and sound, else the
 * level's; sun and sky are always the level's */
Environment environment_for_view(const Environment *base, const ZoneList *zones, Vec3 p);

#endif
