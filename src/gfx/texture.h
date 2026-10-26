/*
 * texture.h — TGA texture loading and nearest-neighbor sampling.
 *
 * Supports uncompressed 24-bit and 32-bit TGA files. Sampling uses
 * nearest-neighbor filtering with UV wrapping, matching the PS1 aesthetic.
 */
#ifndef TEXTURE_H
#define TEXTURE_H

#include <stdint.h>
#include "math3d.h"

typedef struct {
    uint8_t *pixels; // RGB, 3 bytes per pixel
    int width;
    int height;
} Texture;

typedef struct {
    Vec3 color;
    int texture_id; // -1 = no texture
} Material;

int texture_load_tga(Texture *tex, const char *filename);
void texture_free(Texture *tex);
Vec3 texture_sample(const Texture *tex, float u, float v);

#endif
