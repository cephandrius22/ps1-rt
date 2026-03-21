#include "scene.h"
#include <stdlib.h>

void scene_init(Scene *s) {
    s->static_bvh.nodes = NULL;
    s->static_bvh.node_count = 0;
    s->static_bvh.tris = NULL;
    s->static_bvh.tri_count = 0;

    s->dynamic_bvh.nodes = NULL;
    s->dynamic_bvh.node_count = 0;
    s->dynamic_bvh.tris = NULL;
    s->dynamic_bvh.tri_count = 0;
    s->dynamic_tris = NULL;
    s->dynamic_tri_count = 0;
    s->dynamic_tri_cap = 0;
    s->has_dynamic = false;

    entity_list_init(&s->entities);
    entity_meshes_init(&s->entity_meshes);
    light_list_init(&s->lights);

    s->materials = NULL;
    s->textures = NULL;
    s->material_count = 0;
    s->texture_count = 0;
}

void scene_free(Scene *s) {
    bvh_free(&s->static_bvh);
    bvh_free(&s->dynamic_bvh);
    free(s->dynamic_tris);
    s->dynamic_tris = NULL;
    s->dynamic_tri_count = 0;
    s->dynamic_tri_cap = 0;

    entity_list_free(&s->entities);
    entity_meshes_free(&s->entity_meshes);
    light_list_free(&s->lights);

    free(s->materials);
    s->materials = NULL;
    s->material_count = 0;
    /* Note: textures freed by caller (they may be shared) */
}

void scene_build_static(Scene *s, Triangle *tris, int count) {
    bvh_build(&s->static_bvh, tris, count);
}

void scene_rebuild_dynamic(Scene *s) {
    /* Free previous dynamic BVH */
    if (s->has_dynamic) {
        bvh_free(&s->dynamic_bvh);
        s->has_dynamic = false;
    }

    /* Count needed triangles */
    int needed = entity_count_triangles(&s->entities, &s->entity_meshes);
    if (needed == 0) {
        s->dynamic_tri_count = 0;
        return;
    }

    /* Grow buffer if needed */
    if (needed > s->dynamic_tri_cap) {
        int new_cap = needed + needed / 2 + 16;
        Triangle *new_buf = realloc(s->dynamic_tris, (size_t)new_cap * sizeof(Triangle));
        if (!new_buf) return;
        s->dynamic_tris = new_buf;
        s->dynamic_tri_cap = new_cap;
    }

    /* Build transformed triangles */
    s->dynamic_tri_count = entity_build_triangles(
        &s->entities, &s->entity_meshes, s->dynamic_tris, s->dynamic_tri_cap);

    /* Build BVH */
    if (s->dynamic_tri_count > 0) {
        bvh_build(&s->dynamic_bvh, s->dynamic_tris, s->dynamic_tri_count);
        s->has_dynamic = true;
    }
}

HitRecord scene_intersect(const Scene *s, Ray ray, float t_min, float t_max) {
    HitRecord best = bvh_intersect(&s->static_bvh, ray, t_min, t_max);

    if (s->has_dynamic) {
        float dyn_max = best.hit ? best.t : t_max;
        HitRecord dyn = bvh_intersect(&s->dynamic_bvh, ray, t_min, dyn_max);
        if (dyn.hit)
            best = dyn;
    }

    return best;
}

bool scene_occluded(const Scene *s, Ray ray, float t_min, float t_max) {
    HitRecord rec = bvh_intersect(&s->static_bvh, ray, t_min, t_max);
    if (rec.hit) return true;

    if (s->has_dynamic) {
        rec = bvh_intersect(&s->dynamic_bvh, ray, t_min, t_max);
        if (rec.hit) return true;
    }

    return false;
}
