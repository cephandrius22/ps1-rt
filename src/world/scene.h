/*
 * scene.h — Scene container with two-level BVH, entities, and lights.
 *
 * The static BVH holds world geometry (built once). The dynamic BVH holds
 * transformed entity geometry and the sprite list holds billboards, both
 * updated each game tick (the BVH refit in place while the same entities
 * just move, rebuilt when one comes or goes). scene_trace() tests all three and returns the
 * closest hit; its mask decides whether ghost sprites (corpses) count.
 * The nav grid (nav.h) is derived from the static BVH at load.
 */
#ifndef SCENE_H
#define SCENE_H

#include "bvh.h"
#include "entity.h"
#include "light.h"
#include "nav.h"
#include "environment.h"
#include "texture.h"

typedef struct Scene {
    /* Static world geometry */
    BVH static_bvh;

    /* Dynamic entity geometry (rebuilt per tick) */
    BVH dynamic_bvh;
    Triangle *dynamic_tris;
    int dynamic_tri_count;
    int dynamic_tri_cap;
    bool has_dynamic;

    /* Billboards (rebuilt per tick), tested by linear scan */
    Sprite *sprites;
    int sprite_count;
    int sprite_cap;

    /* Entity and template mesh storage */
    EntityList entities;
    EntityMeshes entity_meshes;

    /* Lighting */
    LightList lights;

    /* Materials */
    Material *materials;
    Texture *textures;
    int material_count;
    int texture_count;

    /* Walkable floors for enemy paths and sound (built by level_finish) */
    NavGrid nav;

    /* Sun, ambient, fog, sky: defaults until the level sets them;
     * zones override ambient/fog in parts of the level */
    Environment env;
    ZoneList zones;

    /* Default material per entity type, for entities spawned at runtime
     * (projectiles); filled by level_finish() */
    int type_material[ENTITY_TYPE_COUNT];
} Scene;

/* Empty scene (entity mesh templates built); scene_free releases
 * everything, including materials and textures the level loader added */
void scene_init(Scene *s);
void scene_free(Scene *s);

/* Build the static BVH from world triangles (call once) */
void scene_build_static(Scene *s, Triangle *tris, int count);

/* Pose active entities into the dynamic BVH (call each tick): refit when
 * only positions changed, rebuild otherwise; sprites rebuilt */
void scene_rebuild_dynamic(Scene *s);

/* Add an entity during play: reuses an inactive slot of the same type,
 * so indices stay stable but the list doesn't grow with every rocket.
 * Uses the type's runtime material. Returns its index, or -1. */
int scene_spawn(Scene *s, Entity e);

typedef enum {
    TRACE_VISIBLE, /* everything drawn: rendering, shadows */
    TRACE_SOLID,   /* what blocks movement and bullets: skips ghost sprites
                    * and ghost meshes (pickups, floor pads) */
} TraceMask;

/* Closest hit among static geometry, entity meshes and sprites */
HitRecord scene_trace(const Scene *s, Ray ray, float t_min, float t_max, TraceMask mask);

/* scene_trace() with TRACE_VISIBLE */
HitRecord scene_intersect(const Scene *s, Ray ray, float t_min, float t_max);

/* The sprites a ray may hit: indices into s->sprites. Render culls them
 * per screen tile, so a camera ray tests only the few in front of it. */
typedef struct {
    const int *index;
    int count;
} SpriteSet;

/* scene_intersect() testing only the sprites in set */
HitRecord scene_intersect_in(const Scene *s, Ray ray, float t_min, float t_max,
                             const SpriteSet *set);

/* Shadow test: true if static or entity-mesh geometry occludes the ray in
 * [t_min, t_max]. Sprites don't cast shadows. */
bool scene_occluded(const Scene *s, Ray ray, float t_min, float t_max);

#endif
