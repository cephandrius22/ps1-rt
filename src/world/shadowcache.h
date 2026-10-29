/*
 * shadowcache.h — Which fixed lights each spot of the static world can
 * see, worked out once at level load (Quake-style lightmaps, but of
 * visibility, so flicker and switching still work at render time).
 *
 * Every static triangle gets a grid of sample points in its plane,
 * SHADOW_TEXEL apart; each point holds one bit per cached light: set if
 * nothing static blocks the way to it.
 *
 *      axis_v
 *        ^  ·   ·   ·   ·        · = sample point (bits: lights seen)
 *        |  · /‾‾‾‾‾‾\  ·        points outside the triangle sample
 *        |  ·/ ·   ·  \ ·        its nearest edge instead
 *        | origin ──────────> axis_u
 *
 * A shaded point blends the four samples around it (soft edges, about
 * SHADOW_TEXEL wide). Cached: lights that cast shadows, never move and
 * sit in the first SHADOW_LIGHTS_MAX slots of the scene's list (lights
 * from the level come first; temporary ones are appended after them).
 * Swinging lamps, flashes and the flashlight are still traced per pixel,
 * and so are entity meshes (doors): render tests them on top.
 */
#ifndef SHADOWCACHE_H
#define SHADOWCACHE_H

#include "bvh.h"
#include "light.h"
#include <stdint.h>

#define SHADOW_TEXEL      0.25f  /* meters between sample points */
#define SHADOW_LIGHTS_MAX 32     /* one bit each in a uint32_t */

/* One triangle's grid: point (i, j) = origin + i*T*axis_u + j*T*axis_v */
typedef struct {
    Vec3 origin;
    Vec3 axis_u, axis_v;  /* unit, in the triangle's plane */
    int w, h;             /* sample points across and up */
    int first;            /* index of point (0, 0) in ShadowCache.bits */
} ShadowPatch;

typedef struct {
    ShadowPatch *patches;  /* one per static BVH triangle, same order */
    int patch_count;
    uint32_t *bits;        /* per sample point: bit i = light i visible */
    int bit_count;
    uint32_t cached;       /* bit i = light i is in the cache */
} ShadowCache;

/* Trace every sample point to every cacheable light in range. Call once
 * the static BVH and the level's lights are final. */
void shadowcache_build(ShadowCache *c, const BVH *world, const LightList *lights);
void shadowcache_free(ShadowCache *c);

/* Does the cache answer for light i? */
bool shadowcache_has(const ShadowCache *c, int light);

/* How much of light i reaches point p on static triangle tri (its index
 * in the static BVH): 0 hidden .. 1 visible, blended between samples */
float shadowcache_visible(const ShadowCache *c, int tri, Vec3 p, int light);

#endif
