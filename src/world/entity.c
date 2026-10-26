/*
 * entity.c — Entity storage and type-agnostic dispatch.
 *
 * Nothing here knows about specific types; behavior comes from the
 * EntityDef table in entity_types.c.
 */
#include "entity.h"
#include "kv.h"
#include <stdlib.h>
#include <math.h>

#define ENTITY_LIST_INITIAL_CAP 16
#define USE_FACING_MIN 0.3f /* cos of max angle between view and target */

/* --- Storage --- */

void entity_list_init(EntityList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void entity_list_free(EntityList *list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

int entity_add(EntityList *list, Entity e) {
    if (list->count >= list->capacity) {
        int new_cap = list->capacity == 0 ? ENTITY_LIST_INITIAL_CAP : list->capacity * 2;
        Entity *new_items = realloc(list->items, (size_t)new_cap * sizeof(Entity));
        if (!new_items) {
            return -1;
        }
        list->items = new_items;
        list->capacity = new_cap;
    }
    int idx = list->count++;
    list->items[idx] = e;
    return idx;
}

/* Generic fields come from shared keys (rot, mat); the type reads the rest */
Entity entity_spawn(EntityType type, Vec3 pos, const char *args) {
    Entity e = {
        .type = type,
        .position = pos,
        .active = true,
        .rotation_y = kv_float(args, "rot", 0),
        .material_id = kv_int(args, "mat", 0),
    };

    const EntityDef *def = entity_def(type);
    if (def->spawn) {
        def->spawn(&e, args);
    }
    return e;
}

/* --- Template meshes --- */

void entity_meshes_init(EntityMeshes *em) {
    for (int t = 0; t < ENTITY_TYPE_COUNT; t++) {
        em->initialized[t] = false;
        mesh_init(&em->meshes[t]);

        const EntityDef *def = entity_def((EntityType)t);
        if (def->render != RENDER_MESH || !def->build_mesh) {
            continue;
        }
        def->build_mesh(&em->meshes[t]);
        em->initialized[t] = true;
    }
}

void entity_meshes_free(EntityMeshes *em) {
    for (int t = 0; t < ENTITY_TYPE_COUNT; t++) {
        if (em->initialized[t]) {
            mesh_free(&em->meshes[t]);
        }
        em->initialized[t] = false;
    }
}

/* --- Dispatch --- */

void entity_update(EntityList *list, float dt, float time) {
    for (int i = 0; i < list->count; i++) {
        Entity *e = &list->items[i];
        const EntityDef *def = entity_def(e->type);
        if (!e->active || !def->update) {
            continue;
        }
        def->update(e, dt, time);
    }
}

static bool cylinder_contains(Cylinder c, Vec3 p) {
    if (p.y < c.base.y || p.y > c.base.y + c.height) {
        return false;
    }
    float dx = p.x - c.base.x;
    float dz = p.z - c.base.z;
    return dx * dx + dz * dz < c.radius * c.radius;
}

int entity_touch(EntityList *list, Cylinder body, int *out, int max) {
    int touched = 0;
    for (int i = 0; i < list->count && touched < max; i++) {
        Entity *e = &list->items[i];
        const EntityDef *def = entity_def(e->type);
        if (!e->active || !def->touch) {
            continue;
        }
        if (!cylinder_contains(body, e->position)) {
            continue;
        }
        if (def->touch(e) == TOUCH_CONSUMED) {
            out[touched++] = i;
        }
    }
    return touched;
}

/* Where the entity is this frame; types without pose() sit at position */
static void entity_pose(const Entity *e, Vec3 *pos, float *rot_y) {
    const EntityDef *def = entity_def(e->type);
    if (def->pose) {
        def->pose(e, pos, rot_y);
        return;
    }
    *pos = e->position;
    *rot_y = e->rotation_y;
}

UseResult entity_try_use(EntityList *list, Vec3 pos, Vec3 forward, float range,
                         int *used) {
    Vec3 fwd = vec3(forward.x, 0, forward.z);
    if (vec3_length(fwd) > 0.01f) {
        fwd = vec3_normalize(fwd);
    }

    /* Pick the nearest usable entity roughly in front (horizontal plane) */
    float best_dist = range;
    int best = ENTITY_INDEX_NONE;
    for (int i = 0; i < list->count; i++) {
        const Entity *e = &list->items[i];
        if (!e->active || !entity_def(e->type)->use) {
            continue;
        }

        Vec3 target;
        float rot;
        entity_pose(e, &target, &rot);
        Vec3 to_target = vec3_sub(target, pos);
        to_target.y = 0;

        float dist = vec3_length(to_target);
        if (dist >= best_dist) {
            continue;
        }
        bool on_top = dist < 0.01f; /* direction undefined; accept */
        if (!on_top && vec3_dot(vec3_normalize(to_target), fwd) < USE_FACING_MIN) {
            continue;
        }
        best_dist = dist;
        best = i;
    }

    if (used) {
        *used = best;
    }
    if (best == ENTITY_INDEX_NONE) {
        return USE_NOTHING;
    }
    Entity *e = &list->items[best];
    return entity_def(e->type)->use(e);
}

/* --- Triangle generation --- */

/* Rotate around Y, then translate */
static Vec3 xform_point(Vec3 v, Vec3 pos, float c, float s) {
    return vec3(v.x * c + v.z * s + pos.x, v.y + pos.y, -v.x * s + v.z * c + pos.z);
}

static Triangle transform_triangle(Triangle t, Vec3 pos, float rot_y) {
    float c = cosf(rot_y);
    float s = sinf(rot_y);

    Triangle out = t;
    out.v0 = xform_point(t.v0, pos, c, s);
    out.v1 = xform_point(t.v1, pos, c, s);
    out.v2 = xform_point(t.v2, pos, c, s);
    out.normal = xform_point(t.normal, vec3(0, 0, 0), c, s);
    return out;
}

/* Template mesh for an entity, or NULL if it isn't drawn as a mesh */
static const Mesh *entity_mesh(const Entity *e, const EntityMeshes *em) {
    if (!e->active || entity_def(e->type)->render != RENDER_MESH) {
        return NULL;
    }
    if (!em->initialized[e->type]) {
        return NULL;
    }
    return &em->meshes[e->type];
}

int entity_count_triangles(const EntityList *list, const EntityMeshes *em) {
    int total = 0;
    for (int i = 0; i < list->count; i++) {
        const Mesh *tmpl = entity_mesh(&list->items[i], em);
        if (tmpl) {
            total += tmpl->count;
        }
    }
    return total;
}

int entity_build_triangles(const EntityList *list, const EntityMeshes *em,
                           Triangle *out, int max_tris) {
    int written = 0;
    for (int i = 0; i < list->count; i++) {
        const Entity *e = &list->items[i];
        const Mesh *tmpl = entity_mesh(e, em);
        if (!tmpl) {
            continue;
        }

        Vec3 pos;
        float rot;
        entity_pose(e, &pos, &rot);

        for (int t = 0; t < tmpl->count && written < max_tris; t++) {
            Triangle tri = tmpl->tris[t];
            tri.material_id = e->material_id;
            tri.owner = i + 1;
            out[written++] = transform_triangle(tri, pos, rot);
        }
    }
    return written;
}

int entity_hit(const HitRecord *rec) {
    if (!rec->hit || rec->owner <= 0) {
        return ENTITY_INDEX_NONE;
    }
    return rec->owner - 1;
}
