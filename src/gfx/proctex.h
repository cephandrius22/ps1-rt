/*
 * proctex.h — Procedural texture generation for industrial environments.
 *
 * Generates low-res textures (32x32 or 64x64) in code using hash-based noise.
 * No external image files needed. Sprite textures (barrel, ghoul) use the
 * TEXTURE_KEY color for empty pixels; sheets hold frames side by side.
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
    PTEX_BARREL,      /* sprite: 32x40, color-keyed */
    PTEX_GHOUL,       /* sprite sheet: 5 frames of 32x48, color-keyed */
    PTEX_COUNT
} ProceduralTextureID;

/* Ghoul sheet frame order (columns, left to right) */
typedef enum {
    GHOUL_FRAME_STAND,
    GHOUL_FRAME_WALK,
    GHOUL_FRAME_ATTACK,
    GHOUL_FRAME_PAIN,
    GHOUL_FRAME_DEAD,
    GHOUL_FRAME_COUNT
} GhoulFrame;

/* Name a level or map uses for a texture, e.g. "brick" */
const char *proctex_name(ProceduralTextureID id);

/* Look up texture ID by name string. Returns -1 if not found or "none". */
int proctex_find(const char *name);

/* Generate all procedural textures into the given array.
 * Caller provides space for PTEX_COUNT Textures. */
void proctex_generate_all(Texture *textures);

/* Generate a single procedural texture. */
void proctex_generate(Texture *tex, ProceduralTextureID id);

#endif
