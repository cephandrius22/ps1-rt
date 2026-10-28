/*
 * entity_types.c — Per-type entity behavior and the EntityDef table.
 *
 * Each type supplies only the hooks it needs; entity.c dispatches.
 */
#include "entity.h"
#include "kv.h"
#include "proctex.h" /* GhoulFrame: sprite sheet layout */
#include <math.h>
#include <string.h>
#include <stdio.h>

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

#define AMMO_NAME_DEF     "bullets"
#define WEAPON_NAME_DEF   "shotgun"
#define WEAPON_AMMO_DEF   8.0f   /* ammo that comes with a weapon pickup */

/* Copy a name key into pickup.item (world never interprets it) */
static void pickup_item(Entity *e, const char *args, const char *key, const char *def) {
    if (!kv_string(args, key, e->pickup.item, sizeof(e->pickup.item))) {
        snprintf(e->pickup.item, sizeof(e->pickup.item), "%s", def);
    }
}

static void ammo_spawn(Entity *e, const char *args) {
    e->pickup.amount = kv_float(args, "amount", AMMO_AMOUNT_DEF);
    pickup_item(e, args, "ammo", AMMO_NAME_DEF);
}

static void weapon_spawn(Entity *e, const char *args) {
    e->pickup.amount = kv_float(args, "amount", WEAPON_AMMO_DEF);
    pickup_item(e, args, "give", WEAPON_NAME_DEF);
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

/* Weapon pickups: a long dark box, spinning and bobbing like pickups */
#define WEAPON_PICKUP_LENGTH 0.7f
#define WEAPON_PICKUP_HEIGHT 0.15f
#define WEAPON_PICKUP_DEPTH  0.2f

static void weapon_pickup_mesh(Mesh *m) {
    float hl = WEAPON_PICKUP_LENGTH * 0.5f;
    float hh = WEAPON_PICKUP_HEIGHT * 0.5f;
    float hd = WEAPON_PICKUP_DEPTH * 0.5f;
    make_box_mesh(m, vec3(-hl, -hh, -hd), vec3(hl, hh, hd));
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

/* --- Ghoul: melee enemy sprite. Behavior (sight, chase, attack) is in
 * game/ai.c; here are its body, health and how it looks per state. --- */

#define GHOUL_HEALTH      60.0f
#define GHOUL_WIDTH       1.0f
#define GHOUL_HEIGHT      1.7f
#define GHOUL_WALK_FPS    4.0f  /* stand/walk frame swaps per second */

static void ghoul_spawn(Entity *e, const char *args) {
    e->health = kv_float(args, "health", GHOUL_HEALTH);
    enemy_set_state(e, ENEMY_IDLE);
    e->enemy.cooldown = 0;
}

static DamageResult ghoul_damage(Entity *e, float amount) {
    if (e->enemy.state == ENEMY_DEAD) {
        return DAMAGE_IGNORED;
    }

    e->health -= amount;
    if (e->health > 0) {
        enemy_set_state(e, ENEMY_PAIN);
        return DAMAGE_HURT;
    }
    e->health = 0;
    enemy_set_state(e, ENEMY_DEAD);
    return DAMAGE_KILLED;
}

/* Frame from state; chasing alternates stand/walk. Corpses are ghosts. */
static void ghoul_sprite(const Entity *e, Sprite *out) {
    out->width = GHOUL_WIDTH;
    out->height = GHOUL_HEIGHT;
    out->frame_count = GHOUL_FRAME_COUNT;
    out->solidity = SPRITE_SOLID;

    switch (e->enemy.state) {
    case ENEMY_IDLE:
        out->frame = GHOUL_FRAME_STAND;
        break;
    case ENEMY_CHASE: {
        int step = (int)(e->enemy.state_time * GHOUL_WALK_FPS) % 2;
        out->frame = step ? GHOUL_FRAME_WALK : GHOUL_FRAME_STAND;
        break;
    }
    case ENEMY_ATTACK:
        out->frame = GHOUL_FRAME_ATTACK;
        break;
    case ENEMY_PAIN:
        out->frame = GHOUL_FRAME_PAIN;
        break;
    case ENEMY_DEAD:
        out->frame = GHOUL_FRAME_DEAD;
        out->solidity = SPRITE_GHOST;
        break;
    }
}

/* --- Exit: glowing floor pad; stays put when touched --- */

#define EXIT_HALF_SIZE 0.6f
#define EXIT_THICKNESS 0.05f

_Static_assert(EXIT_THICKNESS < TOUCH_FOOT_SLACK,
               "player standing on the pad must still touch its origin");

static TouchResult exit_touch(Entity *e) {
    (void)e;
    return TOUCH_IGNORED;
}

static void exit_mesh(Mesh *m) {
    make_box_mesh(m, vec3(-EXIT_HALF_SIZE, 0, -EXIT_HALF_SIZE),
                  vec3(EXIT_HALF_SIZE, EXIT_THICKNESS, EXIT_HALF_SIZE));
}

/* --- Projectile: a small glowing ball centered on its position. Ghost:
 * collision traces never stop on it (rockets don't hit each other, and
 * a shooter's aim ray passes through its own shot). --- */

static void projectile_sprite(const Entity *e, Sprite *out) {
    out->width = e->projectile.size;
    out->height = e->projectile.size;
    out->base.y -= e->projectile.size * 0.5f;
    out->frame = e->projectile.frame;
    out->frame_count = PROJ_FRAME_COUNT;
    out->solidity = SPRITE_GHOST;
}

/* --- Level keys per type (defaults shared with the spawn hooks) --- */

static const EntityKey health_keys[] = {
    {"amount", KEY_FLOAT, HEALTH_AMOUNT_DEF, "Health restored", NULL},
    {.type = KEY_END},
};

static const EntityKey ammo_keys[] = {
    {"amount", KEY_FLOAT, AMMO_AMOUNT_DEF, "Rounds given", NULL},
    {"ammo", KEY_STRING, 0, "Ammo type: bullets, shells, rockets", AMMO_NAME_DEF},
    {.type = KEY_END},
};

static const EntityKey weapon_keys[] = {
    {"give", KEY_STRING, 0, "Weapon: shotgun, launcher", WEAPON_NAME_DEF},
    {"amount", KEY_FLOAT, WEAPON_AMMO_DEF, "Ammo that comes with it", NULL},
    {.type = KEY_END},
};

static const EntityKey door_keys[] = {
    {"hinge_x", KEY_FLOAT, 0, ".level only: hinge X (defaults to the door position)", NULL},
    {"hinge_z", KEY_FLOAT, 0, ".level only: hinge Z (defaults to the door position)", NULL},
    {.type = KEY_END},
};

static const EntityKey decor_keys[] = {
    {"w", KEY_LENGTH, DECOR_WIDTH_DEF, "Sprite width", NULL},
    {"h", KEY_LENGTH, DECOR_HEIGHT_DEF, "Sprite height", NULL},
    {"solid", KEY_BOOL, 1, "1 = blocks movement and bullets", NULL},
    {.type = KEY_END},
};

static const EntityKey ghoul_keys[] = {
    {"health", KEY_FLOAT, GHOUL_HEALTH, "Hit points (pistol does 25)", NULL},
    {.type = KEY_END},
};

/* --- Table --- */

static const EntityDef entity_defs[ENTITY_TYPE_COUNT] = {
    [ENTITY_NONE] = { .name = NULL, .render = RENDER_NONE },
    [ENTITY_PICKUP_HEALTH] = {
        .name = "health", .render = RENDER_MESH, .mesh_solidity = SPRITE_GHOST, .touch_effect = TOUCH_EFFECT_HEALTH,
        .default_texture = "none", .default_color = {0.9f, 0.2f, 0.2f},
        .desc = "Health pickup", .keys = health_keys,
        .spawn = health_spawn, .update = pickup_update, .touch = pickup_touch,
        .pose = pickup_pose, .build_mesh = pickup_mesh,
    },
    [ENTITY_PICKUP_AMMO] = {
        .name = "ammo", .render = RENDER_MESH, .mesh_solidity = SPRITE_GHOST, .touch_effect = TOUCH_EFFECT_AMMO,
        .default_texture = "none", .default_color = {0.9f, 0.8f, 0.2f},
        .desc = "Ammo pickup", .keys = ammo_keys,
        .spawn = ammo_spawn, .update = pickup_update, .touch = pickup_touch,
        .pose = pickup_pose, .build_mesh = pickup_mesh,
    },
    [ENTITY_PICKUP_WEAPON] = {
        .name = "weapon", .render = RENDER_MESH, .mesh_solidity = SPRITE_GHOST, .touch_effect = TOUCH_EFFECT_WEAPON,
        .default_texture = "metal_panel", .default_color = {0.4f, 0.45f, 0.5f},
        .desc = "Weapon pickup", .keys = weapon_keys,
        .spawn = weapon_spawn, .update = pickup_update, .touch = pickup_touch,
        .pose = pickup_pose, .build_mesh = weapon_pickup_mesh,
    },
    [ENTITY_DOOR] = {
        .name = "door", .render = RENDER_MESH,
        .default_texture = "rust", .default_color = {0.4f, 0.2f, 0.1f},
        .desc = "Swinging door; E toggles. Place at the hinge.", .keys = door_keys,
        .spawn = door_spawn, .update = door_update, .use = door_use,
        .pose = door_pose, .build_mesh = door_mesh,
    },
    [ENTITY_PROP] = {
        .name = "prop", .render = RENDER_MESH,
        .default_texture = "none", .default_color = {0.5f, 0.5f, 0.5f},
        .desc = "Static cube",
        .build_mesh = prop_mesh,
    },
    [ENTITY_DECOR] = {
        .name = "decor", .render = RENDER_SPRITE,
        .default_texture = "barrel", .default_color = {1, 1, 1},
        .desc = "Decoration sprite (barrel by default; set mat=)", .keys = decor_keys,
        .spawn = decor_spawn, .sprite = decor_sprite,
    },
    [ENTITY_GHOUL] = {
        .name = "ghoul", .render = RENDER_SPRITE,
        .default_texture = "ghoul", .default_color = {1, 1, 1},
        .desc = "Melee enemy", .keys = ghoul_keys,
        .spawn = ghoul_spawn, .damage = ghoul_damage, .sprite = ghoul_sprite,
    },
    [ENTITY_PROJECTILE] = {
        .name = "projectile", .render = RENDER_SPRITE, .runtime_only = true,
        .default_texture = "projectiles", .default_color = {1, 1, 1},
        .desc = "Rocket or spit (spawned by weapons and enemies)",
        .sprite = projectile_sprite,
    },
    [ENTITY_EXIT] = {
        .name = "exit", .render = RENDER_MESH, .mesh_solidity = SPRITE_GHOST, .touch_effect = TOUCH_EFFECT_EXIT,
        .default_texture = "none", .default_color = {0.9f, 0.1f, 0.05f},
        .desc = "Exit pad: touching it ends the level",
        .touch = exit_touch, .build_mesh = exit_mesh,
    },
};

const EntityDef *entity_def(EntityType type) {
    if (type <= ENTITY_NONE || type >= ENTITY_TYPE_COUNT) {
        return &entity_defs[ENTITY_NONE];
    }
    return &entity_defs[type];
}

const EntityKey *entity_key(const EntityDef *def, const char *name) {
    if (!def->keys) {
        return NULL;
    }
    for (const EntityKey *k = def->keys; k->type != KEY_END; k++) {
        if (strcmp(k->name, name) == 0) {
            return k;
        }
    }
    return NULL;
}

bool entity_key_known(const EntityDef *def, const char *name) {
    return strcmp(name, "rot") == 0 || strcmp(name, "mat") == 0 || entity_key(def, name) != NULL;
}

EntityType entity_type_by_name(const char *name) {
    for (int t = ENTITY_NONE + 1; t < ENTITY_TYPE_COUNT; t++) {
        if (strcmp(entity_defs[t].name, name) == 0) {
            return (EntityType)t;
        }
    }
    return ENTITY_NONE;
}
