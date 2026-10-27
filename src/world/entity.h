/*
 * entity.h — Dynamic entities (pickups, doors, props) driven by a type table.
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
    ENTITY_DOOR,
    ENTITY_PROP,
    ENTITY_DECOR,         /* static sprite: barrels, candles, bodies */
    ENTITY_GHOUL,         /* melee enemy; AI lives in game/ai.c */
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
    TOUCH_EFFECT_AMMO,   /* amount = pickup.amount (bullets, for now) */
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

/* Per-type behavior. Any hook may be NULL (= type doesn't do that). */
typedef struct {
    const char *name;       /* level-file keyword, e.g. "health" */
    RenderKind render;
    TouchEffect touch_effect;

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
EntityType entity_type_by_name(const char *name); /* ENTITY_NONE if unknown */

/* Create an entity of a type at pos, letting the type read its args */
Entity entity_spawn(EntityType type, Vec3 pos, const char *args);

void entity_list_init(EntityList *list);
void entity_list_free(EntityList *list);
int  entity_add(EntityList *list, Entity e);

void entity_meshes_init(EntityMeshes *em);
void entity_meshes_free(EntityMeshes *em);

/* Advance all active entities one tick */
void entity_update(EntityList *list, float dt, float time);

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

/* Which entity a ray hit, or ENTITY_INDEX_NONE for world geometry/miss.
 * Entity triangles carry owner = index + 1, so zero-initialized
 * triangles (world, tests) read as "no entity". */
#define ENTITY_INDEX_NONE (-1)
int entity_hit(const HitRecord *rec);

#endif
