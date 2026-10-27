/*
 * sprite.h — Upright billboards (Doom-style enemies and decorations).
 *
 * A sprite is a vertical rectangle that always faces the ray hitting it,
 * so it looks the same from every side. Its texture may hold several
 * frames side by side; color-keyed texels are holes rays pass through.
 *
 *   top view:      ray ──>  |  <- plane turned to face this ray
 *
 *   side view:     +-----+  base.y + height
 *                  |frame|
 *                  +--o--+  base (bottom center)
 *
 * Intersection is analytic (no triangles), so sprites are tested by
 * linear scan in scene_trace(); fine for the tens of sprites a level has.
 */
#ifndef SPRITE_H
#define SPRITE_H

#include "mesh.h"
#include "texture.h"

typedef enum {
    SPRITE_SOLID,  /* blocks movement and bullets (live enemies, barrels) */
    SPRITE_GHOST,  /* drawn only; rays for collision pass through (corpses) */
} SpriteSolidity;

typedef struct {
    Vec3 base;            /* bottom center, world space */
    float width;
    float height;
    int material_id;
    int frame;            /* column in the texture sheet */
    int frame_count;      /* columns in the sheet; 1 = whole texture */
    SpriteSolidity solidity;
    int owner;            /* HitRecord.owner tag (entity index + 1) */
} Sprite;

/* Ray vs sprite. tex may be NULL (solid rectangle, no holes). On a hit
 * closer than t_max, fills rec (normal faces the ray) and returns true. */
bool sprite_intersect(const Sprite *s, const Texture *tex, Ray ray,
                      float t_min, float t_max, HitRecord *rec);

#endif
