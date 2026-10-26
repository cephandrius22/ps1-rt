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
 *   entity_touch()   ──> def->touch()   (walk-over, e.g. pickups)
 *   entity_try_use() ──> def->use()     (E key, e.g. doors)
 *   entity_build_triangles() ──> def->pose() + template mesh (RENDER_MESH)
 *
 * Adding a type = enum value + EntityDef row in entity_types.c.
 */
#ifndef ENTITY_H
#define ENTITY_H

#include "mesh.h"
#include "math3d.h"
#include <stdbool.h>

typedef enum {
    ENTITY_NONE = 0,
    ENTITY_PICKUP_HEALTH,
    ENTITY_PICKUP_AMMO,
    ENTITY_DOOR,
    ENTITY_PROP,
    ENTITY_TYPE_COUNT
} EntityType;

/* How the renderer should draw a type. Sprite drawing is not built yet;
 * the field exists so types can choose without touching generic code. */
typedef enum {
    RENDER_NONE,   /* invisible (triggers, spawn points) */
    RENDER_MESH,   /* template mesh, posed per tick into the dynamic BVH */
    RENDER_SPRITE, /* camera-facing billboard */
} RenderKind;

typedef enum {
    TOUCH_IGNORED,
    TOUCH_CONSUMED, /* entity reacted and was deactivated (e.g. collected) */
} TouchResult;

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
    };

    int material_id;  /* material for this entity's triangles */
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

    /* Fill type-specific fields from level key=values, e.g. "amount=25" */
    void (*spawn)(Entity *e, const char *args);

    void (*update)(Entity *e, float dt, float time);
    TouchResult (*touch)(Entity *e);
    UseResult (*use)(Entity *e);

    /* World placement of the template mesh this frame (bob, hinge swing) */
    void (*pose)(const Entity *e, Vec3 *pos, float *rot_y);

    /* Build the shared template mesh (RENDER_MESH only) */
    void (*build_mesh)(Mesh *m);
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

/* Touch every active entity whose position lies inside body. Writes
 * indices of entities that reacted into out (up to max). Returns how many. */
int entity_touch(EntityList *list, Cylinder body, int *out, int max);

/* Use the nearest usable entity in range and roughly in front.
 * Writes its index to used (if non-NULL). */
UseResult entity_try_use(EntityList *list, Vec3 pos, Vec3 forward, float range,
                         int *used);

/* Write posed triangles for all active RENDER_MESH entities into out.
 * Returns number written. Caller must provide sufficient space. */
int entity_build_triangles(const EntityList *list, const EntityMeshes *em,
                           Triangle *out, int max_tris);

/* Count total triangles needed for all active entities */
int entity_count_triangles(const EntityList *list, const EntityMeshes *em);

/* Which entity a ray hit, or ENTITY_INDEX_NONE for world geometry/miss.
 * Entity triangles carry owner = index + 1, so zero-initialized
 * triangles (world, tests) read as "no entity". */
#define ENTITY_INDEX_NONE (-1)
int entity_hit(const HitRecord *rec);

#endif
