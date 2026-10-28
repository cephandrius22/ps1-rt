/*
 * entity.h — Dynamic entities (pickups, doors, props, decor, enemies, exit)
 * driven by a type table.
 *
 * Entities live in a growable array; indices are stable (inactive entities
 * stay in place). Per-type behavior is not hard-coded in switch statements:
 * each EntityType has an EntityDef with optional hooks. Generic code
 * (update, touch, use, mesh build) just calls whatever hook a type provides.
 *
 *   level "entity health ..." ──name──> EntityDef ──spawn()──> Entity
 *   entity_update()  ──> def->update()
 *   entity_overlap() ──> touchable entities inside a body; the toucher
 *                        decides, then entity_touch() ──> def->touch()
 *   entity_try_use() ──> def->use()     (E key, e.g. doors)
 *   entity_damage()  ──> def->damage()  (bullets, e.g. enemies)
 *   entity_build_triangles() ──> def->pose() + template mesh (RENDER_MESH)
 *   entity_build_sprites()   ──> def->sprite()               (RENDER_SPRITE)
 *
 * Adding a type = enum value + EntityDef row in entity_types.c.
 */
#ifndef ENTITY_H
#define ENTITY_H

#include "mesh.h"
#include "sprite.h"
#include "math3d.h"
#include <stdbool.h>

typedef enum {
    ENTITY_NONE = 0,
    ENTITY_PICKUP_HEALTH,
    ENTITY_PICKUP_AMMO,
    ENTITY_PICKUP_WEAPON,
    ENTITY_DOOR,
    ENTITY_PROP,
    ENTITY_DECOR,         /* static sprite: barrels, candles, bodies */
    ENTITY_GHOUL,         /* melee enemy; AI lives in game/ai.c */
    ENTITY_EXIT,          /* floor pad; touching it ends the level */
    ENTITY_PROJECTILE,    /* rockets, spit: spawned at runtime, moved by game/projectile.c */
    ENTITY_TYPE_COUNT
} EntityType;

/* How the renderer should draw a type. Sprite drawing is not built yet;
 * the field exists so types can choose without touching generic code. */
typedef enum {
    RENDER_NONE,   /* invisible (triggers, spawn points) */
    RENDER_MESH,   /* template mesh, posed per tick into the dynamic BVH */
    RENDER_SPRITE, /* billboard facing each ray (sprite.h) */
} RenderKind;

typedef enum {
    TOUCH_IGNORED,
    TOUCH_CONSUMED, /* entity reacted and was deactivated (e.g. collected) */
} TouchResult;

/* What touching a type means to the toucher. World code only stores it;
 * gameplay interprets it (e.g. heal the player, end the level). */
typedef enum {
    TOUCH_EFFECT_NONE,
    TOUCH_EFFECT_HEALTH, /* amount = pickup.amount */
    TOUCH_EFFECT_AMMO,   /* pickup.item = ammo name, amount = pickup.amount */
    TOUCH_EFFECT_WEAPON, /* pickup.item = weapon name, amount = ammo with it */
    TOUCH_EFFECT_EXIT,   /* end the level */
} TouchEffect;

typedef enum {
    DAMAGE_IGNORED, /* not damageable, or already dead */
    DAMAGE_HURT,
    DAMAGE_KILLED,
} DamageResult;

/* Enemy behavior state. World code stores it and picks sprite frames
 * from it; game/ai.c decides the transitions. */
typedef enum {
    ENEMY_IDLE,    /* waiting until it sees the player */
    ENEMY_CHASE,
    ENEMY_ATTACK,  /* winding up a strike */
    ENEMY_PAIN,    /* flinching after a hit */
    ENEMY_DEAD,
} EnemyState;

typedef enum {
    USE_NOTHING,
    USE_OPENED,
    USE_CLOSED,
} UseResult;

#define ENTITY_ITEM_MAX 16
#define PROJECTILE_OWNER_PLAYER (-2)

typedef struct {
    EntityType type;
    Vec3 position;
    float rotation_y;
    bool active;
    float health;     /* for damageable types; 0 = dead */

    /* Type-specific data */
    union {
        struct {
            float amount;
            char item[ENTITY_ITEM_MAX]; /* what it gives, by name: "shells", "shotgun" */
        } pickup;
        struct {
            float angle;        /* current swing angle in radians */
            float target_angle; /* 0 = closed, ~1.5 = open */
            float swing_speed;
            Vec3 hinge;         /* hinge position (world space) */
        } door;
        struct {
            float width;
            float height;
            SpriteSolidity solidity;
        } decor;
        struct {
            EnemyState state;
            float state_time; /* seconds in the current state */
            float cooldown;   /* seconds until it may attack again */
        } enemy;
        struct {
            int kind;         /* game's ProjectileKind; opaque to world code */
            int frame;        /* ProjectileFrame in the projectiles sheet */
            float size;       /* sprite width = height, meters */
            Vec3 velocity;    /* m/s */
            int owner;        /* shooter's entity index, or PROJECTILE_OWNER_PLAYER */
            float life;       /* seconds until it bursts on its own */
        } projectile;
    };

    int material_id;  /* material for this entity's triangles or sprite */
} Entity;

/* Upright cylinder, e.g. a player's body: base at the feet, extending
 * up by height. Touch tests against this instead of a single point so
 * floor-level pickups are reachable from eye-level cameras. */
typedef struct {
    Vec3 base;
    float radius;
    float height;
} Cylinder;

/* How a level key's value is read. Lengths are meters in .level files
 * and map units in .map files (the .map loader converts). */
typedef enum {
    KEY_FLOAT,
    KEY_INT,
    KEY_BOOL,    /* 0 or 1 */
    KEY_LENGTH,
    KEY_STRING,  /* a name; default in def_string */
    KEY_END,     /* terminates an EntityDef.keys list */
} KeyType;

/* One type-specific key a level may set, e.g. ghoul "health". Used by
 * the .map loader (unit conversion) and the TrenchBroom FGD export. */
typedef struct {
    const char *name;
    KeyType type;
    float def;          /* value when the key is absent */
    const char *desc;
    const char *def_string; /* KEY_STRING default */
} EntityKey;

/* Per-type behavior. Any hook may be NULL (= type doesn't do that). */
typedef struct {
    const char *name;       /* level-file keyword, e.g. "health" */
    RenderKind render;
    TouchEffect touch_effect;

    /* RENDER_MESH types: SPRITE_GHOST meshes are drawn but don't block
     * movement, bullets or sight (pickups, floor pads). Default: solid. */
    SpriteSolidity mesh_solidity;

    /* Material used when a level gives no mat=: a procedural texture name
     * (or "none") plus color, so designers can just place a "ghoul" */
    const char *default_texture;
    Vec3 default_color;

    const char *desc;          /* one line, shown in the editor */
    const EntityKey *keys;     /* type-specific keys, KEY_END-terminated; may be NULL */

    /* Spawned by gameplay, never placed in levels: no FGD entry, and the
     * loader prepares its default material for runtime spawns */
    bool runtime_only;

    /* Fill type-specific fields from level key=values, e.g. "amount=25" */
    void (*spawn)(Entity *e, const char *args);

    void (*update)(Entity *e, float dt, float time);
    TouchResult (*touch)(Entity *e);
    UseResult (*use)(Entity *e);
    DamageResult (*damage)(Entity *e, float amount);

    /* World placement of the template mesh this frame (bob, hinge swing) */
    void (*pose)(const Entity *e, Vec3 *pos, float *rot_y);

    /* Build the shared template mesh (RENDER_MESH only) */
    void (*build_mesh)(Mesh *m);

    /* Size, frame and solidity this tick (RENDER_SPRITE only). Generic
     * code fills base, material and owner beforehand. */
    void (*sprite)(const Entity *e, Sprite *out);
} EntityDef;

typedef struct {
    Entity *items;
    int count;
    int capacity;
} EntityList;

/* Template meshes shared by entity type */
typedef struct {
    Mesh meshes[ENTITY_TYPE_COUNT];
    bool initialized[ENTITY_TYPE_COUNT];
} EntityMeshes;

#define COLLECT_RADIUS   1.0f
#define USE_RANGE        2.5f
#define DOOR_SWING_SPEED 2.5f

/* Type table lookup */
const EntityDef *entity_def(EntityType type);

/* Key metadata for a type, or NULL if the type doesn't declare it */
const EntityKey *entity_key(const EntityDef *def, const char *name);

/* True for keys any entity accepts ("rot", "mat") or that def declares.
 * Loaders warn on the rest; they're usually typos. */
bool entity_key_known(const EntityDef *def, const char *name);
EntityType entity_type_by_name(const char *name); /* ENTITY_NONE if unknown */

/* Create an entity of a type at pos, letting the type read its args.
 * material_id is left 0; the level loader resolves mat= or the default. */
Entity entity_spawn(EntityType type, Vec3 pos, const char *args);

/* Growable entity array. entity_add returns the new (stable) index, or
 * -1 if out of memory. Entities are never removed, only deactivated. */
void entity_list_init(EntityList *list);
void entity_list_free(EntityList *list);
int  entity_add(EntityList *list, Entity e);

/* Build every RENDER_MESH type's template via its build_mesh hook */
void entity_meshes_init(EntityMeshes *em);
void entity_meshes_free(EntityMeshes *em);

/* Advance all active entities one tick */
void entity_update(EntityList *list, float dt, float time);

/* Entities whose origin sits up to this far below the body still count:
 * a floor pad's origin is under the surface the toucher stands on */
#define TOUCH_FOOT_SLACK 0.1f

/* Find active touchable entities whose position lies inside body. Writes
 * their indices into out (up to max). Returns how many. No side effects:
 * the caller decides which to touch (e.g. skip health at full health). */
int entity_overlap(const EntityList *list, Cylinder body, int *out, int max);

/* Run entity i's touch hook (e.g. a pickup deactivates itself) */
TouchResult entity_touch(EntityList *list, int i);

/* Use the nearest usable entity in range and roughly in front.
 * Writes its index to used (if non-NULL). */
UseResult entity_try_use(EntityList *list, Vec3 pos, Vec3 forward, float range,
                         int *used);

/* Damage entity i via its hook (e.g. an enemy flinches or dies) */
DamageResult entity_damage(EntityList *list, int i, float amount);

/* Change state and restart its timer (animations and AI timeouts key off it) */
static inline void enemy_set_state(Entity *e, EnemyState state) {
    e->enemy.state = state;
    e->enemy.state_time = 0;
}

/* Write posed triangles for all active RENDER_MESH entities into out.
 * Returns number written. Caller must provide sufficient space. */
int entity_build_triangles(const EntityList *list, const EntityMeshes *em,
                           Triangle *out, int max_tris);

/* Count total triangles needed for all active entities */
int entity_count_triangles(const EntityList *list, const EntityMeshes *em);

/* Count active RENDER_SPRITE entities */
int entity_count_sprites(const EntityList *list);

/* Write one Sprite per active RENDER_SPRITE entity into out (up to max).
 * Returns number written. */
int entity_build_sprites(const EntityList *list, Sprite *out, int max);

/* Does a hit on this entity's mesh block (movement, bullets, sight)? */
bool entity_mesh_solid(const EntityList *list, int index);

/* Which entity a ray hit, or ENTITY_INDEX_NONE for world geometry/miss.
 * Entity triangles carry owner = index + 1, so zero-initialized
 * triangles (world, tests) read as "no entity". */
#define ENTITY_INDEX_NONE (-1)
int entity_hit(const HitRecord *rec);

#endif
