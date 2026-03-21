#include "entity.h"
#include <stdlib.h>
#include <math.h>

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
        int new_cap = list->capacity == 0 ? 16 : list->capacity * 2;
        Entity *new_items = realloc(list->items, (size_t)new_cap * sizeof(Entity));
        if (!new_items) return -1;
        list->items = new_items;
        list->capacity = new_cap;
    }
    int idx = list->count++;
    list->items[idx] = e;
    return idx;
}

/* --- Template mesh generation --- */

static void make_box_mesh(Mesh *m, Vec3 lo, Vec3 hi) {
    mesh_init(m);
    Vec3 v[8] = {
        vec3(lo.x, lo.y, lo.z), vec3(hi.x, lo.y, lo.z),
        vec3(hi.x, hi.y, lo.z), vec3(lo.x, hi.y, lo.z),
        vec3(lo.x, lo.y, hi.z), vec3(hi.x, lo.y, hi.z),
        vec3(hi.x, hi.y, hi.z), vec3(lo.x, hi.y, hi.z),
    };
    /* 6 faces, 2 tris each */
    int faces[6][4] = {
        {4,5,6,7}, {1,0,3,2}, {0,4,7,3},
        {5,1,2,6}, {7,6,2,3}, {0,1,5,4},
    };
    for (int f = 0; f < 6; f++) {
        Vec3 a = v[faces[f][0]], b = v[faces[f][1]];
        Vec3 c = v[faces[f][2]], d = v[faces[f][3]];
        Vec3 n = vec3_normalize(vec3_cross(vec3_sub(b, a), vec3_sub(c, a)));
        Triangle t1 = {a,b,c, {0,0},{1,0},{1,1}, n, 0};
        Triangle t2 = {a,c,d, {0,0},{1,1},{0,1}, n, 0};
        mesh_add_tri(m, t1);
        mesh_add_tri(m, t2);
    }
}

static void make_diamond_mesh(Mesh *m, float size) {
    mesh_init(m);
    float s = size;
    float h = size * 1.5f;
    Vec3 top    = vec3(0,  h, 0);
    Vec3 bottom = vec3(0, -h, 0);
    Vec3 pts[4] = {
        vec3( s, 0,  0), vec3(0, 0,  s),
        vec3(-s, 0,  0), vec3(0, 0, -s),
    };
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) % 4;
        Vec3 n;
        /* Top half */
        n = vec3_normalize(vec3_cross(vec3_sub(pts[j], pts[i]), vec3_sub(top, pts[i])));
        Triangle t1 = {pts[i], pts[j], top, {0,0},{1,0},{0.5f,1}, n, 0};
        mesh_add_tri(m, t1);
        /* Bottom half */
        n = vec3_normalize(vec3_cross(vec3_sub(bottom, pts[i]), vec3_sub(pts[j], pts[i])));
        Triangle t2 = {pts[i], bottom, pts[j], {0,0},{0.5f,1},{1,0}, n, 0};
        mesh_add_tri(m, t2);
    }
}

void entity_meshes_init(EntityMeshes *em) {
    for (int i = 0; i < ENTITY_TYPE_COUNT; i++) {
        em->initialized[i] = false;
        em->meshes[i].tris = NULL;
        em->meshes[i].count = 0;
        em->meshes[i].capacity = 0;
    }

    /* Pickups: small diamond shape */
    make_diamond_mesh(&em->meshes[ENTITY_PICKUP_HEALTH], 0.2f);
    em->initialized[ENTITY_PICKUP_HEALTH] = true;

    make_diamond_mesh(&em->meshes[ENTITY_PICKUP_AMMO], 0.2f);
    em->initialized[ENTITY_PICKUP_AMMO] = true;

    /* Door: tall flat box (1.2 wide, 2.4 tall, 0.15 thick) centered at origin */
    make_box_mesh(&em->meshes[ENTITY_DOOR], vec3(0, 0, -0.075f), vec3(1.2f, 2.4f, 0.075f));
    em->initialized[ENTITY_DOOR] = true;

    /* Prop: generic cube */
    make_box_mesh(&em->meshes[ENTITY_PROP], vec3(-0.3f, 0, -0.3f), vec3(0.3f, 0.6f, 0.3f));
    em->initialized[ENTITY_PROP] = true;
}

void entity_meshes_free(EntityMeshes *em) {
    for (int i = 0; i < ENTITY_TYPE_COUNT; i++) {
        if (em->initialized[i])
            mesh_free(&em->meshes[i]);
        em->initialized[i] = false;
    }
}

/* --- Update --- */

#define PICKUP_BOB_SPEED  2.0f
#define PICKUP_BOB_HEIGHT 0.15f
#define PICKUP_SPIN_SPEED 2.0f

void entity_update(EntityList *list, float dt, float time) {
    for (int i = 0; i < list->count; i++) {
        Entity *e = &list->items[i];
        if (!e->active) continue;

        switch (e->type) {
        case ENTITY_PICKUP_HEALTH:
        case ENTITY_PICKUP_AMMO:
            e->rotation_y = time * PICKUP_SPIN_SPEED;
            break;
        case ENTITY_DOOR: {
            float diff = e->door.target_angle - e->door.angle;
            if (fabsf(diff) > 0.01f) {
                float step = e->door.swing_speed * dt;
                if (diff > 0)
                    e->door.angle += step < diff ? step : diff;
                else
                    e->door.angle += -step > diff ? -step : diff;
            } else {
                e->door.angle = e->door.target_angle;
            }
            break;
        }
        default:
            break;
        }
    }
}

/* --- Interaction --- */

float entity_try_collect(EntityList *list, Vec3 player_pos, float collect_radius) {
    float total = 0;
    for (int i = 0; i < list->count; i++) {
        Entity *e = &list->items[i];
        if (!e->active) continue;
        if (e->type != ENTITY_PICKUP_HEALTH && e->type != ENTITY_PICKUP_AMMO)
            continue;

        Vec3 diff = vec3_sub(player_pos, e->position);
        float dist = vec3_length(diff);
        if (dist < collect_radius) {
            total += e->pickup.amount;
            e->active = false;
        }
    }
    return total;
}

#define DOOR_OPEN_ANGLE 1.5f  /* ~86 degrees */

int entity_try_use_door(EntityList *list, Vec3 player_pos, Vec3 player_forward, float use_range) {
    float best_dist = use_range;
    Entity *best_door = NULL;

    for (int i = 0; i < list->count; i++) {
        Entity *e = &list->items[i];
        if (!e->active || e->type != ENTITY_DOOR) continue;

        /* Check distance to door hinge */
        Vec3 to_door = vec3_sub(e->door.hinge, player_pos);
        to_door.y = 0;
        float dist = vec3_length(to_door);
        if (dist >= best_dist) continue;

        /* Check if roughly facing the door */
        Vec3 dir = vec3_normalize(to_door);
        Vec3 fwd = player_forward;
        fwd.y = 0;
        if (vec3_length(fwd) > 0.01f) fwd = vec3_normalize(fwd);
        if (vec3_dot(dir, fwd) < 0.3f) continue;

        best_dist = dist;
        best_door = e;
    }

    if (best_door) {
        /* Toggle door open/closed */
        if (best_door->door.target_angle > 0.1f) {
            best_door->door.target_angle = 0;
            return -1; /* closing */
        } else {
            best_door->door.target_angle = DOOR_OPEN_ANGLE;
            return 1; /* opening */
        }
    }
    return 0;
}

/* --- Triangle generation --- */

static Triangle transform_triangle(Triangle t, Vec3 pos, float rot_y) {
    float c = cosf(rot_y), s = sinf(rot_y);
    Triangle out = t;

    /* Rotate around Y then translate */
    #define XFORM(v) do { \
        float nx = (v).x * c + (v).z * s; \
        float nz = -(v).x * s + (v).z * c; \
        (v).x = nx + pos.x; \
        (v).y = (v).y + pos.y; \
        (v).z = nz + pos.z; \
    } while(0)

    XFORM(out.v0);
    XFORM(out.v1);
    XFORM(out.v2);

    /* Rotate normal */
    float nnx = out.normal.x * c + out.normal.z * s;
    float nnz = -out.normal.x * s + out.normal.z * c;
    out.normal.x = nnx;
    out.normal.z = nnz;

    #undef XFORM

    out.material_id = t.material_id;
    return out;
}

int entity_count_triangles(const EntityList *list, const EntityMeshes *em) {
    int total = 0;
    for (int i = 0; i < list->count; i++) {
        const Entity *e = &list->items[i];
        if (!e->active) continue;
        if (e->type <= ENTITY_NONE || e->type >= ENTITY_TYPE_COUNT) continue;
        if (!em->initialized[e->type]) continue;
        total += em->meshes[e->type].count;
    }
    return total;
}

int entity_build_triangles(const EntityList *list, const EntityMeshes *em,
                           Triangle *out, int max_tris) {
    int written = 0;
    for (int i = 0; i < list->count; i++) {
        const Entity *e = &list->items[i];
        if (!e->active) continue;
        if (e->type <= ENTITY_NONE || e->type >= ENTITY_TYPE_COUNT) continue;
        if (!em->initialized[e->type]) continue;

        const Mesh *tmpl = &em->meshes[e->type];
        Vec3 pos = e->position;
        float rot = e->rotation_y;

        /* For pickups, add bobbing offset */
        if (e->type == ENTITY_PICKUP_HEALTH || e->type == ENTITY_PICKUP_AMMO) {
            pos.y += sinf(rot / PICKUP_SPIN_SPEED * PICKUP_BOB_SPEED) * PICKUP_BOB_HEIGHT;
        }

        /* For doors, rotate around hinge point */
        if (e->type == ENTITY_DOOR) {
            pos = e->door.hinge;
            rot = e->door.angle;
        }

        for (int t = 0; t < tmpl->count && written < max_tris; t++) {
            Triangle tri = tmpl->tris[t];
            tri.material_id = e->material_id;
            out[written++] = transform_triangle(tri, pos, rot);
        }
    }
    return written;
}
