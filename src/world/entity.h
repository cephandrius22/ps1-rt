/*
 * entity.h — Dynamic entity system for pickups, doors, and props.
 *
 * Entities are stored in a growable array. Each has a type, transform,
 * and type-specific state. Template meshes are shared across entities
 * of the same type.
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

    int mesh_id;      /* index into template mesh array */
    int material_id;  /* material for this entity's triangles */
} Entity;

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

void entity_list_init(EntityList *list);
void entity_list_free(EntityList *list);
int  entity_add(EntityList *list, Entity e);

#define COLLECT_RADIUS   1.0f
#define USE_RANGE        2.5f
#define DOOR_SWING_SPEED 2.5f

void entity_meshes_init(EntityMeshes *em);
void entity_meshes_free(EntityMeshes *em);

/* Update all entities (bobbing, door animation, etc.) */
void entity_update(EntityList *list, float dt, float time);

/* Check if player is close enough to collect a pickup. Returns amount, deactivates entity. */
float entity_try_collect(EntityList *list, Vec3 player_pos, float collect_radius);

/* Check if player can interact with a door. Opens/closes nearest door in range.
 * Returns: 1 = opened, -1 = closed, 0 = no interaction */
int entity_try_use_door(EntityList *list, Vec3 player_pos, Vec3 player_forward, float use_range);

/* Write transformed triangles for all active entities into output buffer.
 * Returns number of triangles written. Caller must provide sufficient space. */
int entity_build_triangles(const EntityList *list, const EntityMeshes *em,
                           Triangle *out, int max_tris);

/* Count total triangles needed for all active entities */
int entity_count_triangles(const EntityList *list, const EntityMeshes *em);

#endif
