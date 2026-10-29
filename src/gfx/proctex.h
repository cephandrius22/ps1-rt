/*
 * proctex.h — Procedural texture generation for industrial environments.
 *
 * Generates low-res textures (32x32 or 64x64) in code using hash-based noise.
 * No external image files needed. Sprite textures (barrel, enemies) use the
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
    PTEX_GHOUL,       /* sprite sheet: 5 EnemyFrame frames of 32x48, color-keyed */
    PTEX_SPITTER,     /* sprite sheet: 5 EnemyFrame frames of 32x40 */
    PTEX_VIEW_PISTOL, /* viewmodel sheet: 2 frames (ViewFrame) of 64x48 */
    PTEX_VIEW_SHOTGUN,
    PTEX_VIEW_LAUNCHER,
    PTEX_PROJECTILES, /* sprite sheet: ProjectileFrame frames of 16x16 */
    PTEX_KEYCARDS,    /* sprite sheet: KeycardFrame frames of 16x16 */
    PTEX_SLIME,       /* animated surfaces (matlib gives them motion): */
    PTEX_WATER,       /*   tile seamlessly so they can scroll */
    PTEX_SCREEN,
    PTEX_COUNT
} ProceduralTextureID;

/* Enemy sheet frame order, shared by every enemy (columns, left to right) */
typedef enum {
    ENEMY_FRAME_STAND,
    ENEMY_FRAME_WALK,
    ENEMY_FRAME_ATTACK,
    ENEMY_FRAME_PAIN,
    ENEMY_FRAME_DEAD,
    ENEMY_FRAME_COUNT
} EnemyFrame;

/* Name a level or map uses for a texture, e.g. "brick" */
const char *proctex_name(ProceduralTextureID id);

/* Viewmodel sheet frame order */
typedef enum {
    VIEW_FRAME_IDLE,
    VIEW_FRAME_FIRE,
    VIEW_FRAME_COUNT
} ViewFrame;

/* Projectile sheet frame order */
typedef enum {
    PROJ_FRAME_ROCKET,
    PROJ_FRAME_SPIT,
    PROJ_FRAME_COUNT
} ProjectileFrame;

#define PROJ_FRAME_SIZE 16

/* Keycard sheet frame order (= the colors levels name: red, blue, yellow) */
typedef enum {
    KEYCARD_FRAME_RED,
    KEYCARD_FRAME_BLUE,
    KEYCARD_FRAME_YELLOW,
    KEYCARD_FRAME_COUNT
} KeycardFrame;

#define KEYCARD_FRAME_SIZE 16

#define VIEW_FRAME_W 64
#define VIEW_FRAME_H 48

/* Look up texture ID by name string. Returns -1 if not found or "none". */
int proctex_find(const char *name);

/* Generate all procedural textures into the given array.
 * Caller provides space for PTEX_COUNT Textures. */
void proctex_generate_all(Texture *textures);

/* Generate a single procedural texture. */
void proctex_generate(Texture *tex, ProceduralTextureID id);

#endif
