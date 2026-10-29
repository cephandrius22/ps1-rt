/*
 * texture.h — TGA texture loading and nearest-neighbor sampling.
 *
 * Supports uncompressed 24-bit and 32-bit TGA files. Sampling uses
 * nearest-neighbor filtering with UV wrapping, matching the PS1 aesthetic.
 */
#ifndef TEXTURE_H
#define TEXTURE_H

#include <stdint.h>
#include <stdbool.h>
#include "math3d.h"

typedef struct {
    uint8_t *pixels; // RGB, 3 bytes per pixel
    int width;
    int height;
} Texture;

/* A surface's look. The motion fields are zero for still surfaces:
 * slime and water scroll, screens and slime glow (self-lit, so they show
 * in the dark) and can pulse. */
typedef struct {
    Vec3 color;
    int texture_id; // -1 = no texture
    Vec2 scroll;    /* UV units per second */
    float glow;     /* light of its own, added to the scene's (0 = none) */
    float pulse;    /* the glow throbs this many times per second (0 = steady) */
} Material;

/* Load an uncompressed 24/32-bit TGA (alpha dropped). 0 on success, -1 on error. */
int texture_load_tga(Texture *tex, const char *filename);

/* Save as uncompressed 32-bit TGA, color-keyed texels fully transparent
 * (so the editor shows sprite holes). 0 on success, -1 on error. */
int texture_save_tga(const Texture *tex, const char *filename);
void texture_free(Texture *tex);

/* Nearest texel at (u, v), wrapped to [0, 1); v = 0 is the top row.
 * RGB in 0..1; missing pixels return magenta. */
Vec3 texture_sample(const Texture *tex, float u, float v);

/* Sprites use magenta as "no pixel here" (color key, no alpha channel) */
#define TEXTURE_KEY_R 255
#define TEXTURE_KEY_G 0
#define TEXTURE_KEY_B 255

/* False if the texel at (u, v) is the color key */
bool texture_opaque(const Texture *tex, float u, float v);

#endif
