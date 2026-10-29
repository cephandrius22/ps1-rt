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
 *
 *   fog: start ──────── end
 *        clear  blends   fully fog_color
 *
 * Zones override parts of it inside a box, like Doom sectors:
 *
 *   .level: zone x1 y1 z1 x2 y2 z2 ambient=0.01 fog="3 12 0.03 0.05 0.02"
 *   .map:   env_zone brush entity with the same keys (fog in units)
 *
 *   ┌───────────┐┌──────┐   ambient: per surface, from the zone the
 *   │ hall      ││ crypt│   surface is in (a dark room stays dark seen
 *   │ level env ││ zone │   from a bright one); fog: from the zone the
 *   └───────────┘└──────┘   camera is in, blended in by the caller
 */
#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "math3d.h"
#include <stdbool.h>

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
} Environment;

/* What a zone overrides */
typedef enum {
    ZONE_SETS_AMBIENT = 1 << 0,
    ZONE_SETS_FOG     = 1 << 1,
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

/* Fade fog and ambient from a toward b by t (0..1); the rest is b's */
Environment environment_blend(const Environment *a, const Environment *b, float t);

/* Growable zone array; zone_add returns false if out of memory */
void zone_list_init(ZoneList *zones);
void zone_list_free(ZoneList *zones);
bool zone_add(ZoneList *zones, EnvZone zone);

/* A zone over [lo, hi] from ambient= and fog="start end r g b" keys
 * (engine units) applied on top of base. False if neither key is set. */
bool zone_from_keys(EnvZone *zone, Vec3 lo, Vec3 hi, const Environment *base,
                    const char *args);

/* Ambient light for a surface at p: its zone's, else the level's */
float environment_ambient_at(const Environment *base, const ZoneList *zones, Vec3 p);

/* Fog and sky for an eye at p: its zone's fog, else the level's */
Environment environment_for_view(const Environment *base, const ZoneList *zones, Vec3 p);

#endif
