/*
 * scene.h — Scene container with two-level BVH, entities, and lights.
 *
 * The static BVH holds world geometry (built once). The dynamic BVH holds
 * transformed entity geometry and the sprite list holds billboards, both
 * rebuilt each game tick. scene_trace() tests all three and returns the
 * closest hit; its mask decides whether ghost sprites (corpses) count.
 */
#ifndef SCENE_H
#define SCENE_H

#include "bvh.h"
#include "entity.h"
#include "light.h"
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
} Scene;

/* Empty scene (entity mesh templates built); scene_free releases
 * everything, including materials and textures the level loader added */
void scene_init(Scene *s);
void scene_free(Scene *s);

/* Build the static BVH from world triangles (call once) */
void scene_build_static(Scene *s, Triangle *tris, int count);

/* Rebuild dynamic BVH from active entities (call each tick) */
void scene_rebuild_dynamic(Scene *s);

typedef enum {
    TRACE_VISIBLE, /* everything drawn: rendering, shadows */
    TRACE_SOLID,   /* what blocks movement and bullets: skips ghost sprites
                    * and ghost meshes (pickups, floor pads) */
} TraceMask;

/* Closest hit among static geometry, entity meshes and sprites */
HitRecord scene_trace(const Scene *s, Ray ray, float t_min, float t_max, TraceMask mask);

/* scene_trace() with TRACE_VISIBLE */
HitRecord scene_intersect(const Scene *s, Ray ray, float t_min, float t_max);

/* Shadow test: true if static or entity-mesh geometry occludes the ray in
 * [t_min, t_max]. Sprites don't cast shadows. */
bool scene_occluded(const Scene *s, Ray ray, float t_min, float t_max);

#endif
