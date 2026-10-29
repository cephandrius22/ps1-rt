/*
 * light.h — Lights: storage, attenuation, cones, flicker, swing animation.
 *
 * One model for every light: a point that fades to nothing at `radius`,
 * optionally narrowed to a cone (spot). Lamps, muzzle flashes and the
 * player's flashlight all shade through the same code:
 *
 *            aim
 *     ●──────────>      spot: full along `aim`, fading (squared) to
 *      \  cone  /       dark at the cone's edge; point: no cone
 *       \      /
 *
 * Lights are data owned by the scene; render/ does the shading and
 * shadow rays. light_update() animates swinging bulbs each game tick and
 * retires temporary lights (muzzle flashes, explosions), which fade out
 * over their lifetime; flicker is a pure function of time, evaluated
 * when shading.
 */
#ifndef LIGHT_H
#define LIGHT_H

#include "math3d.h"
#include <stdbool.h>

#define MAX_SHADOW_LIGHTS 4  /* max lights that cast shadow rays per pixel */
#define LIGHT_NAME_MAX    32
#define LIGHT_FALLOFF_DEF 0.5f /* per meter, inside the radius window */

typedef enum {
    LIGHT_POINT,  /* zero-init default */
    LIGHT_SPOT,
} LightKind;

typedef struct {
    LightKind kind;
    Vec3 position;
    Vec3 color;       /* RGB, each 0-1 */
    float intensity;
    float radius;     /* no light at or beyond this distance */
    float falloff;    /* how fast it dims per meter; 0 = LIGHT_FALLOFF_DEF */

    /* LIGHT_SPOT only */
    Vec3 aim;         /* cone axis, normalized */
    float cone_cos;   /* cos of the cone's half-angle (its dark edge) */

    bool cast_shadows;
    float flicker_speed;  /* 0 = no flicker */
    float flicker_amount; /* 0-1, how much intensity varies */

    /* Swing animation (pendulum on a cord) — set swing_speed > 0 to enable */
    Vec3 anchor;       /* ceiling attachment point */
    float cord_length; /* distance from anchor to bulb */
    float swing_speed; /* angular frequency (rad/s) */
    float swing_angle; /* max swing angle in radians */

    /* Temporary lights: removed when ttl runs out, fading meanwhile */
    float ttl;         /* seconds left; 0 = permanent */
    float ttl_total;   /* lifetime it started with (for the fade) */

    /* Switchable: a trigger whose target is this name toggles it */
    char targetname[LIGHT_NAME_MAX];
    bool off;          /* gives no light (zero-init = on) */

    /* Shootable: a bullet through the bulb puts it out for good */
    bool breakable;
    bool broken;       /* shot out; triggers can't switch it back on */
} Light;

typedef struct {
    Light *items;
    int count;
    int capacity;
} LightList;

/* Growable light array; light_add returns the index, or -1 if out of
 * memory. Indices are not stable: temporary lights are swap-removed. */
void light_list_init(LightList *list);
void light_list_free(LightList *list);
int  light_add(LightList *list, Light light);

/* Add a light that fades out and disappears after ttl seconds */
int  light_add_temporary(LightList *list, Light light, float ttl);

/* Light at pos with shared optional keys from a level line or map entity:
 * shadows, flicker_speed, flicker_amount, swing_speed, swing_angle, cord,
 * targetname, off (1 = starts switched off), breakable (1 = bullets can
 * shoot the bulb out), cone (half-angle in degrees;
 * > 0 makes a spot) and aim_x/aim_y/aim_z (spot axis, default straight
 * down). The swing anchor defaults to `cord` straight above pos. */
Light light_from_keys(Vec3 pos, Vec3 color, float intensity, float radius,
                           const char *args);

/* Animate swinging lights and age temporary ones by dt. Call once per
 * game tick. */
void light_update(LightList *list, float time, float dt);

/* Get effective intensity for a light at a given time (applies flicker
 * and a temporary light's fade; 0 when switched off) */
float light_effective_intensity(const Light *light, float time);

/* Compute attenuation factor based on distance and light radius */
float light_attenuation(const Light *light, float distance);

/* Radius of the visible bulb: what render draws and bullets can hit */
float light_bulb_radius(const Light *light);

/* The nearest lit, breakable, permanent bulb the ray passes through
 * before t_max: its index (valid until temporary lights next expire), or
 * -1. Writes the distance along the ray to *t. */
int light_bulb_hit(const LightList *lights, Ray ray, float t_max, float *t);

/* A spot light: cone of half_angle (radians) around aim */
Light light_spot(Vec3 pos, Vec3 aim, Vec3 color, float intensity, float radius,
                 float half_angle);

/* How much of the light reaches along dir (from the light, normalized):
 * 1 for point lights; for spots 1 on the axis down to 0 at the edge */
float light_cone(const Light *light, Vec3 dir);

#endif
