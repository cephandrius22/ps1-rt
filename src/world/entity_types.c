/*
 * entity_types.c — Per-type entity behavior and the EntityDef table.
 *
 * Each type supplies only the hooks it needs; entity.c dispatches.
 */
#include "entity.h"
#include "kv.h"
#include "proctex.h" /* EnemyFrame: sprite sheet layout */
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

/* --- Movers (func_door, func_button): brushes that slide along one
 * axis. Doors default to Doom style, up into the ceiling; buttons sink
 * into the wall along the angle arrow. game/logic.c moves them; here is
 * only their shape and pose. --- */

#define DOOR_SPEED_DEF    2.5f   /* m/s */
#define DOOR_WAIT_DEF     4.0f   /* seconds open before closing */
#define DOOR_LIP_DEF      0.125f /* meters left showing when fully open */
#define DOOR_OPENS_DEF    "up"
#define BUTTON_SPEED_DEF  1.0f
#define BUTTON_WAIT_DEF   1.0f   /* seconds pressed before popping back */
#define BUTTON_LIP_DEF    0.125f
#define BUTTON_OPENS_DEF  "side"

/* Direction from the opens= key: up, down, or side (along rot, the
 * editor's angle arrow; yaw 0 faces -Z) */
static Vec3 door_dir(const Entity *e, const char *opens) {
    if (strcmp(opens, "down") == 0) {
        return vec3(0, -1, 0);
    }
    if (strcmp(opens, "side") == 0) {
        return vec3(-sinf(e->rotation_y), 0, -cosf(e->rotation_y));
    }
    return vec3(0, 1, 0);
}

/* Defaults per mover type */
typedef struct {
    const char *opens;
    float speed, wait, lip;
} MoverDefaults;

static const MoverDefaults door_defaults = {DOOR_OPENS_DEF, DOOR_SPEED_DEF, DOOR_WAIT_DEF, DOOR_LIP_DEF};
static const MoverDefaults button_defaults = {BUTTON_OPENS_DEF, BUTTON_SPEED_DEF, BUTTON_WAIT_DEF,
                                              BUTTON_LIP_DEF};

/* Travel = the mover's own size along its direction, minus the lip:
 *
 *    ┌──┐ ↑ open: lip still in the doorway
 *    │  │ │ travel
 *    └──┘ ┘
 */
static void mover_spawn(Entity *e, const char *args, const MoverDefaults *def) {
    char opens[ENTITY_ITEM_MAX];
    snprintf(opens, sizeof(opens), "%s", def->opens);
    kv_string(args, "opens", opens, sizeof(opens));
    kv_string(args, "key", e->mover.key, sizeof(e->mover.key));
    e->mover.toggle = kv_int(args, "toggle", 0) != 0;

    Vec3 dir = door_dir(e, opens);
    Vec3 size = vec3_mul(e->half_size, 2.0f);
    float extent = fabsf(dir.x) * size.x + fabsf(dir.y) * size.y + fabsf(dir.z) * size.z;
    float lip = kv_float(args, "lip", def->lip);

    e->mover.state = MOVER_CLOSED;
    e->mover.dir = dir;
    e->mover.travel = fmaxf(extent - lip, 0.0f);
    e->mover.speed = kv_float(args, "speed", def->speed);
    e->mover.wait = kv_float(args, "wait", def->wait);
}

static void door_spawn(Entity *e, const char *args) {
    mover_spawn(e, args, &door_defaults);
}

static void button_spawn(Entity *e, const char *args) {
    mover_spawn(e, args, &button_defaults);
}

/* --- Triggers: invisible brushes; game/logic.c fires them when the
 * player is inside. trigger_once never re-arms. --- */

#define TRIGGER_WAIT_DEF 1.0f
#define TRIGGER_ONCE     (-1.0f)

static void trigger_once_spawn(Entity *e, const char *args) {
    (void)args;
    e->trigger.wait = TRIGGER_ONCE;
}

#define HURT_DAMAGE_DEF 10.0f /* per second */

static void trigger_hurt_spawn(Entity *e, const char *args) {
    e->trigger.damage = kv_float(args, "damage", HURT_DAMAGE_DEF);
}

static void trigger_multiple_spawn(Entity *e, const char *args) {
    e->trigger.wait = fmaxf(kv_float(args, "wait", TRIGGER_WAIT_DEF), 0.0f);
}

static void mover_pose(const Entity *e, Vec3 *pos, float *rot_y) {
    *pos = vec3_add(e->position, vec3_mul(e->mover.dir, e->mover.offset));
    *rot_y = 0; /* the model is already in world orientation */
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

/* --- Enemies: shared body logic. Behavior (sight, chase, attack) is in
 * game/ai.c; here are health, flinch/death, and frames per state. --- */

#define ENEMY_WALK_FPS    4.0f  /* stand/walk frame swaps per second */

/* A named enemy waits hidden (inactive) until a trigger targets it */
static void enemy_spawn(Entity *e, const char *args, float default_health) {
    e->health = kv_float(args, "health", default_health);
    enemy_set_state(e, ENEMY_IDLE);
    e->enemy.cooldown = 0;
    e->active = e->targetname[0] == '\0';
}

static DamageResult enemy_damage(Entity *e, float amount) {
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

/* Frame from state (EnemyFrame sheet); chasing alternates stand/walk.
 * Corpses are ghosts. */
static void enemy_sprite(const Entity *e, Sprite *out, float width, float height) {
    out->width = width;
    out->height = height;
    out->frame_count = ENEMY_FRAME_COUNT;
    out->solidity = SPRITE_SOLID;

    switch (e->enemy.state) {
    case ENEMY_IDLE:
        out->frame = ENEMY_FRAME_STAND;
        break;
    case ENEMY_CHASE: {
        int step = (int)(e->enemy.state_time * ENEMY_WALK_FPS) % 2;
        out->frame = step ? ENEMY_FRAME_WALK : ENEMY_FRAME_STAND;
        break;
    }
    case ENEMY_ATTACK:
        out->frame = ENEMY_FRAME_ATTACK;
        break;
    case ENEMY_PAIN:
        out->frame = ENEMY_FRAME_PAIN;
        break;
    case ENEMY_DEAD:
        out->frame = ENEMY_FRAME_DEAD;
        out->solidity = SPRITE_GHOST;
        break;
    }
}

/* Ghoul: tall melee enemy */
#define GHOUL_HEALTH  60.0f
#define GHOUL_WIDTH   1.0f
#define GHOUL_HEIGHT  1.7f

static void ghoul_spawn(Entity *e, const char *args) {
    enemy_spawn(e, args, GHOUL_HEALTH);
}

static void ghoul_sprite(const Entity *e, Sprite *out) {
    enemy_sprite(e, out, GHOUL_WIDTH, GHOUL_HEIGHT);
}

/* Spitter: hunched ranged enemy */
#define SPITTER_HEALTH  45.0f
#define SPITTER_WIDTH   1.1f
#define SPITTER_HEIGHT  1.4f

static void spitter_spawn(Entity *e, const char *args) {
    enemy_spawn(e, args, SPITTER_HEALTH);
}

static void spitter_sprite(const Entity *e, Sprite *out) {
    enemy_sprite(e, out, SPITTER_WIDTH, SPITTER_HEIGHT);
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

/* --- Keycard: a small floating card sprite, bobbing like a pickup.
 * The color names are the level's contract with game/inventory.c; here
 * they only pick the sheet frame. --- */

#define KEYCARD_COLOR_DEF "red"
#define KEYCARD_SIZE      0.35f
#define KEYCARD_FLOAT     0.4f  /* sprite bottom above the origin */

static const char *keycard_frames[KEYCARD_FRAME_COUNT] = {"red", "blue", "yellow"};

static void keycard_spawn(Entity *e, const char *args) {
    pickup_item(e, args, "color", KEYCARD_COLOR_DEF);
}

static void keycard_sprite(const Entity *e, Sprite *out) {
    out->width = KEYCARD_SIZE;
    out->height = KEYCARD_SIZE;
    out->frame_count = KEYCARD_FRAME_COUNT;
    out->solidity = SPRITE_GHOST;
    for (int f = 0; f < KEYCARD_FRAME_COUNT; f++) {
        if (strcmp(e->pickup.item, keycard_frames[f]) == 0) {
            out->frame = f;
        }
    }

    float bob = sinf(e->rotation_y / PICKUP_SPIN_SPEED * PICKUP_BOB_SPEED) * PICKUP_BOB_HEIGHT;
    out->base.y += KEYCARD_FLOAT + bob;
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
    {"opens", KEY_STRING, 0, "Direction: up, down, or side (along the angle)", DOOR_OPENS_DEF},
    {"speed", KEY_LENGTH, DOOR_SPEED_DEF, "Speed per second", NULL},
    {"wait", KEY_FLOAT, DOOR_WAIT_DEF, "Seconds open before closing (-1 = stays open)", NULL},
    {"lip", KEY_LENGTH, DOOR_LIP_DEF, "Left showing when open", NULL},
    {"key", KEY_STRING, 0, "Keycard needed: red, blue, yellow (empty = none)", ""},
    {"targetname", KEY_STRING, 0, "Name; a named door opens only when triggered", ""},
    {"toggle", KEY_BOOL, 0, "Named door: each firing flips it open/shut", NULL},
    {"target", KEY_STRING, 0, "Fires everything with this name when it opens", ""},
    {"delay", KEY_FLOAT, 0, "Seconds before firing its target", NULL},
    {.type = KEY_END},
};

static const EntityKey button_keys[] = {
    {"target", KEY_STRING, 0, "Fires everything with this name", ""},
    {"message", KEY_STRING, 0, "Shown when pressed", ""},
    {"opens", KEY_STRING, 0, "Pressed in: side (along the angle), up, down", BUTTON_OPENS_DEF},
    {"speed", KEY_LENGTH, BUTTON_SPEED_DEF, "Speed per second", NULL},
    {"wait", KEY_FLOAT, BUTTON_WAIT_DEF, "Seconds before it pops back (-1 = stays in)", NULL},
    {"lip", KEY_LENGTH, BUTTON_LIP_DEF, "Left showing when pressed", NULL},
    {"key", KEY_STRING, 0, "Keycard needed: red, blue, yellow (empty = none)", ""},
    {"delay", KEY_FLOAT, 0, "Seconds before firing its target", NULL},
    {.type = KEY_END},
};

static const EntityKey trigger_once_keys[] = {
    {"target", KEY_STRING, 0, "Fires everything with this name", ""},
    {"message", KEY_STRING, 0, "Shown when entered", ""},
    {"delay", KEY_FLOAT, 0, "Seconds before firing its target", NULL},
    {.type = KEY_END},
};

static const EntityKey trigger_hurt_keys[] = {
    {"damage", KEY_FLOAT, HURT_DAMAGE_DEF, "Damage per second while inside", NULL},
    {.type = KEY_END},
};

static const EntityKey trigger_relay_keys[] = {
    {"targetname", KEY_STRING, 0, "Fires when this name is fired", ""},
    {"target", KEY_STRING, 0, "Fires everything with this name", ""},
    {"delay", KEY_FLOAT, 0, "Seconds before passing it on", NULL},
    {"message", KEY_STRING, 0, "Shown when it fires", ""},
    {.type = KEY_END},
};

static const EntityKey trigger_secret_keys[] = {
    {"target", KEY_STRING, 0, "Fires everything with this name when found", ""},
    {"message", KEY_STRING, 0, "Shown when found (empty = \"A secret area!\")", ""},
    {"delay", KEY_FLOAT, 0, "Seconds before firing its target", NULL},
    {.type = KEY_END},
};

static const EntityKey trigger_multiple_keys[] = {
    {"target", KEY_STRING, 0, "Fires everything with this name", ""},
    {"message", KEY_STRING, 0, "Shown when entered", ""},
    {"wait", KEY_FLOAT, TRIGGER_WAIT_DEF, "Seconds before it can fire again", NULL},
    {"delay", KEY_FLOAT, 0, "Seconds before firing its target", NULL},
    {.type = KEY_END},
};

static const EntityKey keycard_keys[] = {
    {"color", KEY_STRING, 0, "Card color: red, blue, yellow", KEYCARD_COLOR_DEF},
    {.type = KEY_END},
};

static const EntityKey decor_keys[] = {
    {"w", KEY_LENGTH, DECOR_WIDTH_DEF, "Sprite width", NULL},
    {"h", KEY_LENGTH, DECOR_HEIGHT_DEF, "Sprite height", NULL},
    {"solid", KEY_BOOL, 1, "1 = blocks movement and bullets", NULL},
    {.type = KEY_END},
};

#define HIDDEN_DESC "Name; a named monster stays hidden until triggered"

static const EntityKey ghoul_keys[] = {
    {"health", KEY_FLOAT, GHOUL_HEALTH, "Hit points (pistol does 25)", NULL},
    {"targetname", KEY_STRING, 0, HIDDEN_DESC, ""},
    {.type = KEY_END},
};

static const EntityKey spitter_keys[] = {
    {"health", KEY_FLOAT, SPITTER_HEALTH, "Hit points (pistol does 25)", NULL},
    {"targetname", KEY_STRING, 0, HIDDEN_DESC, ""},
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
        .name = "func_door", .render = RENDER_MESH, .brush = true, .usable = true,
        .default_texture = "metal_panel", .default_color = {0.4f, 0.2f, 0.1f},
        .desc = "Door brush: E opens (Doom-style, rises by default)", .keys = door_keys,
        .spawn = door_spawn, .pose = mover_pose,
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
        .spawn = ghoul_spawn, .damage = enemy_damage, .sprite = ghoul_sprite,
    },
    [ENTITY_SPITTER] = {
        .name = "spitter", .render = RENDER_SPRITE,
        .default_texture = "spitter", .default_color = {1, 1, 1},
        .desc = "Ranged enemy: keeps its distance, spits", .keys = spitter_keys,
        .spawn = spitter_spawn, .damage = enemy_damage, .sprite = spitter_sprite,
    },
    [ENTITY_PROJECTILE] = {
        .name = "projectile", .render = RENDER_SPRITE, .runtime_only = true,
        .default_texture = "projectiles", .default_color = {1, 1, 1},
        .desc = "Rocket or spit (spawned by weapons and enemies)",
        .sprite = projectile_sprite,
    },
    [ENTITY_KEYCARD] = {
        .name = "keycard", .render = RENDER_SPRITE, .touch_effect = TOUCH_EFFECT_KEYCARD,
        .default_texture = "keycards", .default_color = {1, 1, 1},
        .desc = "Keycard: opens doors with a matching key", .keys = keycard_keys,
        .spawn = keycard_spawn, .update = pickup_update, .touch = pickup_touch,
        .sprite = keycard_sprite,
    },
    [ENTITY_BUTTON] = {
        .name = "func_button", .render = RENDER_MESH, .brush = true, .usable = true,
        .default_texture = "metal_panel", .default_color = {0.7f, 0.6f, 0.2f},
        .desc = "Button brush: E presses it and fires its target", .keys = button_keys,
        .spawn = button_spawn, .pose = mover_pose,
    },
    [ENTITY_TRIGGER_ONCE] = {
        .name = "trigger_once", .render = RENDER_NONE, .brush = true,
        .default_texture = "none", .default_color = {0.8f, 0.5f, 0.1f},
        .desc = "Invisible brush: fires its target the first time the player enters",
        .keys = trigger_once_keys, .spawn = trigger_once_spawn,
    },
    [ENTITY_TRIGGER_SECRET] = {
        .name = "trigger_secret", .render = RENDER_NONE, .brush = true,
        .default_texture = "none", .default_color = {0.9f, 0.8f, 0.2f},
        .desc = "Invisible brush around a secret area: counted for the intermission",
        .keys = trigger_secret_keys, .spawn = trigger_once_spawn,
    },
    [ENTITY_TRIGGER_HURT] = {
        .name = "trigger_hurt", .render = RENDER_NONE, .brush = true,
        .default_texture = "none", .default_color = {0.9f, 0.2f, 0.1f},
        .desc = "Invisible brush: hurts the player while inside (slime, fire)",
        .keys = trigger_hurt_keys, .spawn = trigger_hurt_spawn,
    },
    [ENTITY_TRIGGER_RELAY] = {
        .name = "trigger_relay", .render = RENDER_NONE,
        .default_texture = "none", .default_color = {0.6f, 0.6f, 0.9f},
        .desc = "Point: when its name fires, fires its own target after delay",
        .keys = trigger_relay_keys,
    },
    [ENTITY_TRIGGER_MULTIPLE] = {
        .name = "trigger_multiple", .render = RENDER_NONE, .brush = true,
        .default_texture = "none", .default_color = {0.8f, 0.5f, 0.1f},
        .desc = "Invisible brush: fires its target while the player is inside, every wait",
        .keys = trigger_multiple_keys, .spawn = trigger_multiple_spawn,
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
