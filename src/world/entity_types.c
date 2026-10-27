/*
 * entity_types.c — Per-type entity behavior and the EntityDef table.
 *
 * Each type supplies only the hooks it needs; entity.c dispatches.
 */
#include "entity.h"
#include "kv.h"
#include <math.h>
#include <string.h>

/* --- Template meshes --- */

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
        Triangle t1 = {a,b,c, {0,0},{1,0},{1,1}, n, 0, 0};
        Triangle t2 = {a,c,d, {0,0},{1,1},{0,1}, n, 0, 0};
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
        Triangle t1 = {pts[i], pts[j], top, {0,0},{1,0},{0.5f,1}, n, 0, 0};
        mesh_add_tri(m, t1);
        /* Bottom half */
        n = vec3_normalize(vec3_cross(vec3_sub(bottom, pts[i]), vec3_sub(pts[j], pts[i])));
        Triangle t2 = {pts[i], bottom, pts[j], {0,0},{0.5f,1},{1,0}, n, 0, 0};
        mesh_add_tri(m, t2);
    }
}

/* --- Pickups: spin, bob, collected on touch --- */

#define PICKUP_SIZE         0.2f
#define PICKUP_BOB_SPEED    2.0f
#define PICKUP_BOB_HEIGHT   0.15f
#define PICKUP_SPIN_SPEED   2.0f
#define HEALTH_AMOUNT_DEF   25.0f
#define AMMO_AMOUNT_DEF     10.0f

static void health_spawn(Entity *e, const char *args) {
    e->pickup.amount = kv_float(args, "amount", HEALTH_AMOUNT_DEF);
}

static void ammo_spawn(Entity *e, const char *args) {
    e->pickup.amount = kv_float(args, "amount", AMMO_AMOUNT_DEF);
}

static void pickup_update(Entity *e, float dt, float time) {
    (void)dt;
    e->rotation_y = time * PICKUP_SPIN_SPEED;
}

static TouchResult pickup_touch(Entity *e) {
    e->active = false;
    return TOUCH_CONSUMED;
}

/* Bob phase derives from spin angle, so it needs no extra state */
static void pickup_pose(const Entity *e, Vec3 *pos, float *rot_y) {
    *pos = e->position;
    *rot_y = e->rotation_y;
    pos->y += sinf(e->rotation_y / PICKUP_SPIN_SPEED * PICKUP_BOB_SPEED) * PICKUP_BOB_HEIGHT;
}

static void pickup_mesh(Mesh *m) {
    make_diamond_mesh(m, PICKUP_SIZE);
}

/* --- Doors: swing around a hinge toward a target angle --- */

#define DOOR_OPEN_ANGLE    1.5f   /* ~86 degrees */
#define DOOR_CLOSED_ANGLE  0.0f
#define DOOR_SNAP_EPSILON  0.01f
#define DOOR_WIDTH         1.2f
#define DOOR_HEIGHT        2.4f
#define DOOR_THICKNESS     0.15f

static void door_spawn(Entity *e, const char *args) {
    e->door.angle = DOOR_CLOSED_ANGLE;
    e->door.target_angle = DOOR_CLOSED_ANGLE;
    e->door.swing_speed = DOOR_SWING_SPEED;
    e->door.hinge = vec3(kv_float(args, "hinge_x", e->position.x), 0,
                         kv_float(args, "hinge_z", e->position.z));
}

static void door_update(Entity *e, float dt, float time) {
    (void)time;
    float diff = e->door.target_angle - e->door.angle;
    if (fabsf(diff) <= DOOR_SNAP_EPSILON) {
        e->door.angle = e->door.target_angle;
        return;
    }

    /* Step toward target without overshooting */
    float step = e->door.swing_speed * dt;
    if (diff > 0) {
        e->door.angle += step < diff ? step : diff;
    } else {
        e->door.angle += -step > diff ? -step : diff;
    }
}

static UseResult door_use(Entity *e) {
    bool is_open = e->door.target_angle > DOOR_SNAP_EPSILON;
    if (is_open) {
        e->door.target_angle = DOOR_CLOSED_ANGLE;
        return USE_CLOSED;
    }
    e->door.target_angle = DOOR_OPEN_ANGLE;
    return USE_OPENED;
}

static void door_pose(const Entity *e, Vec3 *pos, float *rot_y) {
    *pos = e->door.hinge;
    *rot_y = e->door.angle;
}

/* Mesh extends from the hinge (x=0) to the free edge (x=width) */
static void door_mesh(Mesh *m) {
    float half_t = DOOR_THICKNESS * 0.5f;
    make_box_mesh(m, vec3(0, 0, -half_t), vec3(DOOR_WIDTH, DOOR_HEIGHT, half_t));
}

/* --- Props: static cube --- */

#define PROP_HALF_SIZE 0.3f
#define PROP_HEIGHT    0.6f

static void prop_mesh(Mesh *m) {
    make_box_mesh(m, vec3(-PROP_HALF_SIZE, 0, -PROP_HALF_SIZE),
                  vec3(PROP_HALF_SIZE, PROP_HEIGHT, PROP_HALF_SIZE));
}

/* --- Decor: static sprite (barrel, candle, corpse) --- */

#define DECOR_WIDTH_DEF  0.8f
#define DECOR_HEIGHT_DEF 1.0f

/* Keys: w=, h= (world units), solid=0 to let the player walk through */
static void decor_spawn(Entity *e, const char *args) {
    e->decor.width = kv_float(args, "w", DECOR_WIDTH_DEF);
    e->decor.height = kv_float(args, "h", DECOR_HEIGHT_DEF);
    e->decor.solidity = kv_int(args, "solid", 1) ? SPRITE_SOLID : SPRITE_GHOST;
}

static void decor_sprite(const Entity *e, Sprite *out) {
    out->width = e->decor.width;
    out->height = e->decor.height;
    out->solidity = e->decor.solidity;
}

/* --- Table --- */

static const EntityDef entity_defs[ENTITY_TYPE_COUNT] = {
    [ENTITY_NONE] = { .name = NULL, .render = RENDER_NONE },
    [ENTITY_PICKUP_HEALTH] = {
        .name = "health", .render = RENDER_MESH, .touch_effect = TOUCH_EFFECT_HEALTH,
        .spawn = health_spawn, .update = pickup_update, .touch = pickup_touch,
        .pose = pickup_pose, .build_mesh = pickup_mesh,
    },
    [ENTITY_PICKUP_AMMO] = {
        .name = "ammo", .render = RENDER_MESH, .touch_effect = TOUCH_EFFECT_AMMO,
        .spawn = ammo_spawn, .update = pickup_update, .touch = pickup_touch,
        .pose = pickup_pose, .build_mesh = pickup_mesh,
    },
    [ENTITY_DOOR] = {
        .name = "door", .render = RENDER_MESH,
        .spawn = door_spawn, .update = door_update, .use = door_use,
        .pose = door_pose, .build_mesh = door_mesh,
    },
    [ENTITY_PROP] = {
        .name = "prop", .render = RENDER_MESH,
        .build_mesh = prop_mesh,
    },
    [ENTITY_DECOR] = {
        .name = "decor", .render = RENDER_SPRITE,
        .spawn = decor_spawn, .sprite = decor_sprite,
    },
};

const EntityDef *entity_def(EntityType type) {
    if (type <= ENTITY_NONE || type >= ENTITY_TYPE_COUNT) {
        return &entity_defs[ENTITY_NONE];
    }
    return &entity_defs[type];
}

EntityType entity_type_by_name(const char *name) {
    for (int t = ENTITY_NONE + 1; t < ENTITY_TYPE_COUNT; t++) {
        if (strcmp(entity_defs[t].name, name) == 0) {
            return (EntityType)t;
        }
    }
    return ENTITY_NONE;
}
