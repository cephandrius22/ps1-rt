/*
 * proctex_internal.h — Helpers shared by the proctex*.c generator files.
 * Not part of the gfx API: only proctex*.c include it.
 *
 *   proctex.c          names, lookup, industrial set, sprites, animated
 *   proctex_church.c   church and crypt surfaces
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

static inline void tex_set_key(Texture *tex, int x, int y) {
    tex_set(tex, x, y, TEXTURE_KEY_R, TEXTURE_KEY_G, TEXTURE_KEY_B);
}

/* --- Sprite sheets (color-keyed) --- */

/* A texture holding equal-size frames side by side (sprite sheets) */
typedef struct {
    Texture *tex;
    int frame_w;
    int frame_h;
} Sheet;

/* Allocate a frames-wide sheet, every pixel the color key */
static inline Sheet sheet_alloc(Texture *tex, int frame_w, int frame_h, int frames) {
    tex_alloc(tex, frame_w * frames, frame_h);
    for (int y = 0; y < frame_h; y++) {
        for (int x = 0; x < tex->width; x++) {
            tex_set_key(tex, x, y);
        }
    }
    return (Sheet){tex, frame_w, frame_h};
}

/* Paint into frame `f` of a sheet; out-of-frame pixels are clipped.
 * Skin gets per-pixel noise so flat shapes read as flesh. */
static inline void sheet_px(const Sheet *sh, int f, int x, int y, Rgb c, float noise) {
    if (x < 0 || x >= sh->frame_w || y < 0 || y >= sh->frame_h) {
        return;
    }
    float k = 1.0f - noise * noise01(f * 97 + x * 7, y * 13);
    tex_set(sh->tex, f * sh->frame_w + x, y, clamp8(c.r * k), clamp8(c.g * k), clamp8(c.b * k));
}

static inline void sheet_rect(const Sheet *sh, int f, int x0, int y0, int x1, int y1, Rgb c, float noise) {
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            sheet_px(sh, f, x, y, c, noise);
        }
    }
}

static inline void sheet_ellipse(const Sheet *sh, int f, float cx, float cy, float rx, float ry,
                          Rgb c, float noise) {
    for (int y = (int)(cy - ry); y <= (int)(cy + ry); y++) {
        for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
            float dx = (x + 0.5f - cx) / rx;
            float dy = (y + 0.5f - cy) / ry;
            if (dx * dx + dy * dy <= 1.0f) {
                sheet_px(sh, f, x, y, c, noise);
            }
        }
    }
}

/* --- Church set (proctex_church.c) --- */

void church_ashlar(Texture *tex);
void church_rubble(Texture *tex);
void church_flagstone(Texture *tex);
void church_tomb_slab(Texture *tex);
void church_plaster(Texture *tex);
void church_oak_panel(Texture *tex);
void church_oak_planks(Texture *tex);
void church_marble(Texture *tex);
void church_carpet(Texture *tex);
void church_iron(Texture *tex);
void church_stained_glass(Texture *tex);
void church_rose_window(Texture *tex);
void church_candelabra(Texture *tex);   /* sprites */
void church_gravestone(Texture *tex);
void church_statue(Texture *tex);

#endif
