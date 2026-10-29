#include "shadowcache.h"
#include <math.h>
#include <stdlib.h>

#define SHADOW_BIAS  0.01f   /* sample this far off the surface, toward the light */
#define SHADOW_T_MIN 0.001f

/* Closest point on triangle abc to p (Ericson, Real-Time Collision
 * Detection 5.1.5): samples outside the triangle move onto it, so they
 * never sit inside a neighboring wall */
static Vec3 closest_on_tri(Vec3 p, Vec3 a, Vec3 b, Vec3 c) {
    Vec3 ab = vec3_sub(b, a);
    Vec3 ac = vec3_sub(c, a);
    Vec3 ap = vec3_sub(p, a);
    float d1 = vec3_dot(ab, ap);
    float d2 = vec3_dot(ac, ap);
    if (d1 <= 0 && d2 <= 0) {
        return a;
    }

    Vec3 bp = vec3_sub(p, b);
    float d3 = vec3_dot(ab, bp);
    float d4 = vec3_dot(ac, bp);
    if (d3 >= 0 && d4 <= d3) {
        return b;
    }
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) {
        return vec3_add(a, vec3_mul(ab, d1 / (d1 - d3)));
    }

    Vec3 cp = vec3_sub(p, c);
    float d5 = vec3_dot(ab, cp);
    float d6 = vec3_dot(ac, cp);
    if (d6 >= 0 && d5 <= d6) {
        return c;
    }
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) {
        return vec3_add(a, vec3_mul(ac, d2 / (d2 - d6)));
    }
    float va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return vec3_add(b, vec3_mul(vec3_sub(c, b), w));
    }

    float denom = 1.0f / (va + vb + vc);
    return vec3_add(a, vec3_add(vec3_mul(ab, vb * denom), vec3_mul(ac, vc * denom)));
}

/* Lights the cache can answer for: shadowed, fixed in place, permanent */
static uint32_t cacheable_lights(const LightList *lights) {
    uint32_t mask = 0;
    for (int i = 0; i < lights->count && i < SHADOW_LIGHTS_MAX; i++) {
        const Light *l = &lights->items[i];
        if (l->cast_shadows && l->swing_speed <= 0 && l->ttl_total <= 0) {
            mask |= 1u << i;
        }
    }
    return mask;
}

/* Lay a grid over the triangle's bounding rectangle in its own plane */
static ShadowPatch make_patch(const Triangle *t, int first) {
    Vec3 u = vec3_normalize(vec3_sub(t->v1, t->v0));
    Vec3 n = vec3_normalize(vec3_cross(vec3_sub(t->v1, t->v0), vec3_sub(t->v2, t->v0)));
    Vec3 v = vec3_cross(n, u);

    Vec3 verts[3] = {t->v0, t->v1, t->v2};
    float u0 = INFINITY, u1 = -INFINITY, v0 = INFINITY, v1 = -INFINITY;
    for (int k = 0; k < 3; k++) {
        Vec3 d = vec3_sub(verts[k], t->v0);
        u0 = fminf(u0, vec3_dot(d, u));
        u1 = fmaxf(u1, vec3_dot(d, u));
        v0 = fminf(v0, vec3_dot(d, v));
        v1 = fmaxf(v1, vec3_dot(d, v));
    }

    return (ShadowPatch){
        .origin = vec3_add(t->v0, vec3_add(vec3_mul(u, u0), vec3_mul(v, v0))),
        .axis_u = u, .axis_v = v,
        .w = (int)ceilf((u1 - u0) / SHADOW_TEXEL) + 1,
        .h = (int)ceilf((v1 - v0) / SHADOW_TEXEL) + 1,
        .first = first,
    };
}

/* Bits of the lights in mask that sample point p (on triangle t) sees */
static uint32_t sample_bits(const BVH *world, const LightList *lights, uint32_t mask,
                            const Triangle *t, Vec3 p) {
    Vec3 n = vec3_normalize(vec3_cross(vec3_sub(t->v1, t->v0), vec3_sub(t->v2, t->v0)));
    uint32_t seen = 0;
    for (int i = 0; i < lights->count && i < SHADOW_LIGHTS_MAX; i++) {
        if (!(mask & (1u << i))) {
            continue;
        }
        const Light *l = &lights->items[i];
        Vec3 to = vec3_sub(l->position, p);
        float dist = vec3_length(to);
        if (dist >= l->radius) {
            continue; /* out of reach: never shaded, any bit will do */
        }

        /* Start on the light's side of the surface (shading is two-sided) */
        float side = vec3_dot(n, to) >= 0 ? 1.0f : -1.0f;
        Vec3 from = vec3_add(p, vec3_mul(n, SHADOW_BIAS * side));
        Vec3 dir = vec3_normalize(vec3_sub(l->position, from));
        float reach = vec3_length(vec3_sub(l->position, from)) - SHADOW_BIAS;
        if (!bvh_occluded(world, (Ray){from, dir}, SHADOW_T_MIN, reach)) {
            seen |= 1u << i;
        }
    }
    return seen;
}

void shadowcache_build(ShadowCache *c, const BVH *world, const LightList *lights) {
    *c = (ShadowCache){.cached = cacheable_lights(lights)};
    if (world->tri_count == 0 || c->cached == 0) {
        return;
    }

    c->patches = malloc(sizeof(ShadowPatch) * (size_t)world->tri_count);
    if (!c->patches) {
        c->cached = 0;
        return;
    }
    int total = 0;
    for (int i = 0; i < world->tri_count; i++) {
        c->patches[i] = make_patch(&world->tris[i], total);
        total += c->patches[i].w * c->patches[i].h;
    }
    c->patch_count = world->tri_count;

    c->bits = malloc(sizeof(uint32_t) * (size_t)total);
    if (!c->bits) {
        shadowcache_free(c);
        return;
    }
    c->bit_count = total;

    for (int i = 0; i < c->patch_count; i++) {
        const ShadowPatch *pa = &c->patches[i];
        const Triangle *t = &world->tris[i];
        for (int y = 0; y < pa->h; y++) {
            for (int x = 0; x < pa->w; x++) {
                Vec3 q = vec3_add(pa->origin, vec3_add(vec3_mul(pa->axis_u, x * SHADOW_TEXEL),
                                                       vec3_mul(pa->axis_v, y * SHADOW_TEXEL)));
                Vec3 p = closest_on_tri(q, t->v0, t->v1, t->v2);
                c->bits[pa->first + y * pa->w + x] = sample_bits(world, lights, c->cached, t, p);
            }
        }
    }
}

void shadowcache_free(ShadowCache *c) {
    free(c->patches);
    free(c->bits);
    *c = (ShadowCache){0};
}

bool shadowcache_has(const ShadowCache *c, int light) {
    return light >= 0 && light < SHADOW_LIGHTS_MAX && (c->cached & (1u << light));
}

/* 1 if sample point (x, y) of patch pa sees the light, else 0 */
static float seen(const ShadowCache *c, const ShadowPatch *pa, int x, int y, uint32_t bit) {
    return (c->bits[pa->first + y * pa->w + x] & bit) ? 1.0f : 0.0f;
}

float shadowcache_visible(const ShadowCache *c, int tri, Vec3 p, int light) {
    const ShadowPatch *pa = &c->patches[tri];
    uint32_t bit = 1u << light;
    Vec3 d = vec3_sub(p, pa->origin);

    /* Grid coordinates, clamped so the 2x2 block stays on the grid */
    float fx = fminf(fmaxf(vec3_dot(d, pa->axis_u) / SHADOW_TEXEL, 0), (float)(pa->w - 1));
    float fy = fminf(fmaxf(vec3_dot(d, pa->axis_v) / SHADOW_TEXEL, 0), (float)(pa->h - 1));
    int x0 = pa->w > 1 ? (int)fminf(fx, (float)(pa->w - 2)) : 0;
    int y0 = pa->h > 1 ? (int)fminf(fy, (float)(pa->h - 2)) : 0;
    int x1 = pa->w > 1 ? x0 + 1 : x0;
    int y1 = pa->h > 1 ? y0 + 1 : y0;
    float tx = fx - x0;
    float ty = fy - y0;

    float top = seen(c, pa, x0, y0, bit) * (1 - tx) + seen(c, pa, x1, y0, bit) * tx;
    float bottom = seen(c, pa, x0, y1, bit) * (1 - tx) + seen(c, pa, x1, y1, bit) * tx;
    return top * (1 - ty) + bottom * ty;
}
