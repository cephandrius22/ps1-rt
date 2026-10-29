/*
 * proctex_internal.h — Helpers shared by the proctex*.c generator files.
 * Not part of the gfx API: only proctex*.c include it.
 *
 *   proctex.c          names, lookup, industrial set, sprites, animated
 */
#ifndef PROCTEX_INTERNAL_H
#define PROCTEX_INTERNAL_H

#include "proctex.h"
#include <stdlib.h>

typedef struct {
    uint8_t r, g, b;
} Rgb;

/* --- Hash-based noise --- */

/* Unsigned math: it must wrap, and signed overflow is undefined
 * (GCC optimizes on it; textures would differ between compilers) */
static inline uint32_t hash2d(int x, int y) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177;
    return h ^ (h >> 16);
}

static inline float noise01(int x, int y) {
    return (float)(hash2d(x, y) & 0xFFFF) / 65535.0f;
}

/* --- Pixels --- */

static inline void tex_alloc(Texture *tex, int w, int h) {
    tex->width = w;
    tex->height = h;
    tex->pixels = malloc((size_t)(w * h * 3));
}

static inline void tex_set(Texture *tex, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    int i = (y * tex->width + x) * 3;
    tex->pixels[i] = r;
    tex->pixels[i + 1] = g;
    tex->pixels[i + 2] = b;
}

static inline uint8_t clamp8(float v) {
    if (v < 0) {
        return 0;
    }
    if (v > 255) {
        return 255;
    }
    return (uint8_t)v;
}

#endif
