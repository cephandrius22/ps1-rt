/*
 * scene.h — Scene container with two-level BVH, entities, and lights.
 *
 * The static BVH holds world geometry (built once). The dynamic BVH holds
 * transformed entity geometry (rebuilt each frame). scene_intersect()
 * tests both and returns the closer hit.
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

    /* Dynamic entity geometry (rebuilt per frame) */
    BVH dynamic_bvh;
    Triangle *dynamic_tris;
    int dynamic_tri_count;
    int dynamic_tri_cap;
    bool has_dynamic;

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

void scene_init(Scene *s);
void scene_free(Scene *s);

/* Build the static BVH from world triangles (call once) */
void scene_build_static(Scene *s, Triangle *tris, int count);

/* Rebuild dynamic BVH from active entities (call each frame) */
void scene_rebuild_dynamic(Scene *s);

/* Intersect ray against both BVHs, return closer hit */
HitRecord scene_intersect(const Scene *s, Ray ray, float t_min, float t_max);

/* Shadow test: returns true if any geometry occludes the ray in [t_min, t_max] */
bool scene_occluded(const Scene *s, Ray ray, float t_min, float t_max);

#endif
