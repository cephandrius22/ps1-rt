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

    s->sprites = NULL;
    s->sprite_count = 0;
    s->sprite_cap = 0;

    entity_list_init(&s->entities);
    entity_meshes_init(&s->entity_meshes);
    light_list_init(&s->lights);

    s->materials = NULL;
    s->textures = NULL;
    s->material_count = 0;
    s->texture_count = 0;
    for (int t = 0; t < ENTITY_TYPE_COUNT; t++) {
        s->type_material[t] = 0;
    }
    s->nav = (NavGrid){0};
    s->env = environment_default();
    zone_list_init(&s->zones);
}

void scene_free(Scene *s) {
    bvh_free(&s->static_bvh);
    bvh_free(&s->dynamic_bvh);
    free(s->dynamic_tris);
    s->dynamic_tris = NULL;
    s->dynamic_tri_count = 0;
    s->dynamic_tri_cap = 0;

    free(s->sprites);
    s->sprites = NULL;
    s->sprite_count = 0;
    s->sprite_cap = 0;

    entity_list_free(&s->entities);
    entity_meshes_free(&s->entity_meshes);
    light_list_free(&s->lights);
    nav_free(&s->nav);
    zone_list_free(&s->zones);

    free(s->materials);
    s->materials = NULL;
    s->material_count = 0;

    for (int i = 0; i < s->texture_count; i++) {
        texture_free(&s->textures[i]);
    }
    free(s->textures);
    s->textures = NULL;
    s->texture_count = 0;
}

void scene_build_static(Scene *s, Triangle *tris, int count) {
    bvh_build(&s->static_bvh, tris, count);
}

/* Refill the sprite list from RENDER_SPRITE entities */
static void rebuild_sprites(Scene *s) {
    int needed = entity_count_sprites(&s->entities);
    if (needed > s->sprite_cap) {
        Sprite *buf = realloc(s->sprites, (size_t)needed * sizeof(Sprite));
        if (!buf) {
            s->sprite_count = 0;
            return;
        }
        s->sprites = buf;
        s->sprite_cap = needed;
    }
    s->sprite_count = entity_build_sprites(&s->entities, s->sprites, s->sprite_cap);
}

void scene_rebuild_dynamic(Scene *s) {
    rebuild_sprites(s);

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
        if (!new_buf) {
            return;
        }
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

/* Texture behind a material, or NULL (untextured / out of range) */
static const Texture *material_texture(const Scene *s, int material_id) {
    if (material_id < 0 || material_id >= s->material_count) {
        return NULL;
    }
    int tex = s->materials[material_id].texture_id;
    if (tex < 0 || tex >= s->texture_count) {
        return NULL;
    }
    return &s->textures[tex];
}

/* Closest sprite hit nearer than t_max; ghosts skipped for TRACE_SOLID */
static bool trace_sprites(const Scene *s, Ray ray, float t_min, float t_max,
                          TraceMask mask, HitRecord *best) {
    bool hit = false;
    for (int i = 0; i < s->sprite_count; i++) {
        const Sprite *sp = &s->sprites[i];
        if (mask == TRACE_SOLID && sp->solidity == SPRITE_GHOST) {
            continue;
        }

        HitRecord rec;
        const Texture *tex = material_texture(s, sp->material_id);
        if (sprite_intersect(sp, tex, ray, t_min, t_max, &rec)) {
            *best = rec;
            t_max = rec.t;
            hit = true;
        }
    }
    return hit;
}

#define GHOST_SKIP_MAX 8      /* ghost meshes a solid trace may pass in a row */
#define GHOST_SKIP_EPS 1e-4f  /* restart just past a skipped hit */

/* Closest entity-mesh hit; TRACE_SOLID passes through ghost meshes
 * (pickups, pads) by re-tracing from just beyond them */
static HitRecord trace_dynamic(const Scene *s, Ray ray, float t_min, float t_max, TraceMask mask) {
    for (int i = 0; i < GHOST_SKIP_MAX; i++) {
        HitRecord hit = bvh_intersect(&s->dynamic_bvh, ray, t_min, t_max);
        if (!hit.hit || mask == TRACE_VISIBLE ||
            entity_mesh_solid(&s->entities, entity_hit(&hit))) {
            return hit;
        }
        t_min = hit.t + GHOST_SKIP_EPS;
    }
    return (HitRecord){.hit = false, .t = t_max};
}

HitRecord scene_trace(const Scene *s, Ray ray, float t_min, float t_max, TraceMask mask) {
    HitRecord best = bvh_intersect(&s->static_bvh, ray, t_min, t_max);

    if (s->has_dynamic) {
        float dyn_max = best.hit ? best.t : t_max;
        HitRecord dyn = trace_dynamic(s, ray, t_min, dyn_max, mask);
        if (dyn.hit) {
            best = dyn;
        }
    }

    trace_sprites(s, ray, t_min, best.hit ? best.t : t_max, mask, &best);
    return best;
}

HitRecord scene_intersect(const Scene *s, Ray ray, float t_min, float t_max) {
    return scene_trace(s, ray, t_min, t_max, TRACE_VISIBLE);
}

bool scene_occluded(const Scene *s, Ray ray, float t_min, float t_max) {
    if (bvh_occluded(&s->static_bvh, ray, t_min, t_max)) {
        return true;
    }
    if (s->has_dynamic && bvh_occluded(&s->dynamic_bvh, ray, t_min, t_max)) {
        return true;
    }

    /* Sprites cast no shadows: a billboard re-faces every ray, so its
     * own shadow ray could clip it (and Doom sprites never did) */
    return false;
}

int scene_spawn(Scene *s, Entity e) {
    e.material_id = s->type_material[e.type];
    EntityList *list = &s->entities;
    for (int i = 0; i < list->count; i++) {
        if (!list->items[i].active && list->items[i].type == e.type) {
            list->items[i] = e;
            return i;
        }
    }
    return entity_add(list, e);
}
