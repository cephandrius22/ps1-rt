/*
 * proctex.h — Procedural texture generation for industrial environments.
 *
 * Generates low-res textures (32x32 or 64x64) in code using hash-based noise.
 * No external image files needed.
 */
#ifndef PROCTEX_H
#define PROCTEX_H

#include "texture.h"

typedef enum {
    PTEX_CONCRETE,
    PTEX_BRICK,
    PTEX_METAL_PANEL,
    PTEX_METAL_GRATE,
    PTEX_RUST,
    PTEX_FLOOR_TILE,
    PTEX_PIPE,
    PTEX_COUNT
} ProceduralTextureID;

/* Look up texture ID by name string. Returns -1 if not found or "none". */
int proctex_find(const char *name);

/* Generate all procedural textures into the given array.
 * Caller provides space for PTEX_COUNT Textures. */
void proctex_generate_all(Texture *textures);

/* Generate a single procedural texture. */
void proctex_generate(Texture *tex, ProceduralTextureID id);

#endif
