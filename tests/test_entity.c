#include "test_framework.h"
#include <string.h>
#include "entity.h"
#include <stdlib.h>

TEST(test_entity_list_init) {
    EntityList list;
    entity_list_init(&list);
    ASSERT(list.count == 0);
    ASSERT(list.items == NULL);
    entity_list_free(&list);
}

TEST(test_entity_add) {
    EntityList list;
    entity_list_init(&list);

    Entity e = {.type = ENTITY_PICKUP_HEALTH, .position = vec3(1, 2, 3), .active = true};
    int idx = entity_add(&list, e);
    ASSERT(idx == 0);
    ASSERT(list.count == 1);
    ASSERT_NEAR(list.items[0].position.x, 1.0f, 1e-6f);

    entity_list_free(&list);
}

TEST(test_entity_add_grows) {
    EntityList list;
    entity_list_init(&list);

    for (int i = 0; i < 100; i++) {
        Entity e = {.type = ENTITY_PROP, .active = true};
        entity_add(&list, e);
    }
    ASSERT(list.count == 100);
    ASSERT(list.capacity >= 100);

    entity_list_free(&list);
}

TEST(test_entity_meshes_init) {
    EntityMeshes em;
    entity_meshes_init(&em);

    ASSERT(em.initialized[ENTITY_PICKUP_HEALTH]);
    ASSERT(em.initialized[ENTITY_PICKUP_AMMO]);
    ASSERT(!em.initialized[ENTITY_DOOR]); /* brush: per-entity models */
    ASSERT(em.initialized[ENTITY_PROP]);
    ASSERT(em.meshes[ENTITY_PICKUP_HEALTH].count > 0);

    entity_meshes_free(&em);
}

TEST(test_entity_collect_pickup) {
    EntityList list;
    entity_list_init(&list);

    Entity health = {
        .type = ENTITY_PICKUP_HEALTH, .position = vec3(0, 0.5f, 0),
        .active = true, .pickup = {.amount = 25}
    };
    entity_add(&list, health);

    int found[4];

    /* Player too far — nothing overlaps */
    Cylinder far = {.base = vec3(5, 0, 0), .radius = COLLECT_RADIUS, .height = 1.6f};
    ASSERT(entity_overlap(&list, far, found, 4) == 0);

    /* Close enough: overlap has no side effects until touched */
    Cylinder near = {.base = vec3(0, 0, 0), .radius = COLLECT_RADIUS, .height = 1.6f};
    ASSERT(entity_overlap(&list, near, found, 4) == 1);
    ASSERT(found[0] == 0);
    ASSERT(list.items[0].active);

    ASSERT(entity_touch(&list, found[0]) == TOUCH_CONSUMED);
    ASSERT(!list.items[0].active);
    ASSERT(entity_touch(&list, found[0]) == TOUCH_IGNORED);

    entity_list_free(&list);
}

/* A 2 m wide, 2.5 m tall door brush centered at (0, 1.25, z) */
static Entity door_at(float z, const char *args) {
    return entity_spawn_brush(ENTITY_DOOR, vec3(0, 1.25f, z), vec3(1, 1.25f, 0.1f), args);
}

TEST(test_entity_door_usable) {
    EntityList list;
    entity_list_init(&list);
    entity_add(&list, door_at(-2, ""));
    entity_add(&list, door_at(-10, ""));

    /* Facing the near door, within USE_RANGE; the far one is out of range */
    Vec3 eye = vec3(0, 1.6f, 0);
    ASSERT_EQ_INT(entity_find_usable(&list, eye, vec3(0, 0, -1), USE_RANGE), 0);
    ASSERT_EQ_INT(entity_find_usable(&list, eye, vec3(0, 0, 1), USE_RANGE), ENTITY_INDEX_NONE);

    /* A wide door is used from its edge: 1.9 m off its center, 0.1 m from its end */
    Vec3 aside = vec3(1.9f, 1.6f, -0.5f);
    ASSERT_EQ_INT(entity_find_usable(&list, aside, vec3(0, 0, -1), USE_RANGE), 0);

    entity_list_free(&list);
}

/* Travel = size along the direction, minus the lip; sideways follows rot */
TEST(test_entity_door_shape) {
    Entity up = door_at(0, "lip=0.5");
    ASSERT_NEAR(up.mover.travel, 2.0f, 1e-5f);
    ASSERT_EQ_INT(up.mover.state, MOVER_CLOSED);

    Entity side = door_at(0, "opens=side rot=-1.5707963 lip=0");
    ASSERT_NEAR(side.mover.dir.x, 1.0f, 1e-5f); /* yaw -90° faces +X */
    ASSERT_NEAR(side.mover.travel, 2.0f, 1e-5f);

    /* Posed bounds follow the slide */
    up.mover.offset = 1.0f;
    Vec3 lo, hi;
    entity_bounds(&up, &lo, &hi);
    ASSERT_NEAR(lo.y, 1.0f, 1e-5f);
    ASSERT_NEAR(hi.y, 3.5f, 1e-5f);
}

TEST(test_entity_build_triangles) {
    EntityList list;
    entity_list_init(&list);

    EntityMeshes em;
    entity_meshes_init(&em);

    Entity pickup = {
        .type = ENTITY_PICKUP_HEALTH, .position = vec3(1, 0.5f, 2),
        .rotation_y = 0, .active = true, .material_id = 3
    };
    entity_add(&list, pickup);

    int count = entity_count_triangles(&list, &em);
    ASSERT(count > 0);

    Triangle *tris = malloc((size_t)count * sizeof(Triangle));
    int written = entity_build_triangles(&list, &em, tris, count);
    ASSERT(written == count);

    /* Triangles should be translated to entity position */
    bool has_offset = false;
    for (int i = 0; i < written; i++) {
        if (tris[i].v0.x > 0.5f || tris[i].v0.z > 1.5f) {
            has_offset = true;
        }
    }
    ASSERT(has_offset);

    /* Material ID should be set */
    ASSERT(tris[0].material_id == 3);

    free(tris);
    entity_list_free(&list);
    entity_meshes_free(&em);
}

TEST(test_entity_inactive_skipped) {
    EntityList list;
    entity_list_init(&list);
    EntityMeshes em;
    entity_meshes_init(&em);

    Entity pickup = {
        .type = ENTITY_PICKUP_HEALTH, .position = vec3(0, 0, 0),
        .active = false
    };
    entity_add(&list, pickup);

    int count = entity_count_triangles(&list, &em);
    ASSERT(count == 0);

    entity_list_free(&list);
    entity_meshes_free(&em);
}

TEST(test_entity_type_by_name) {
    ASSERT_EQ_INT(entity_type_by_name("health"), ENTITY_PICKUP_HEALTH);
    ASSERT_EQ_INT(entity_type_by_name("ammo"), ENTITY_PICKUP_AMMO);
    ASSERT_EQ_INT(entity_type_by_name("func_door"), ENTITY_DOOR);
    ASSERT_EQ_INT(entity_type_by_name("prop"), ENTITY_PROP);
    ASSERT_EQ_INT(entity_type_by_name("dragon"), ENTITY_NONE);
}

TEST(test_entity_spawn_reads_args) {
    Entity hp = entity_spawn(ENTITY_PICKUP_HEALTH, vec3(1, 2, 3), "amount=40 rot=1.5");
    ASSERT(hp.active);
    ASSERT_NEAR(hp.position.y, 2.0f, 1e-6f);
    ASSERT_NEAR(hp.pickup.amount, 40.0f, 1e-6f);
    ASSERT_NEAR(hp.rotation_y, 1.5f, 1e-6f);
    ASSERT_EQ_INT(hp.material_id, 0); /* resolved later by the level loader */

    /* Shared keys any type may carry */
    Entity named = entity_spawn(ENTITY_PROP, vec3(0, 0, 0), "target=gate targetname=lever");
    ASSERT(strcmp(named.target, "gate") == 0);
    ASSERT(strcmp(named.targetname, "lever") == 0);
}

TEST(test_entity_hookless_type_is_inert) {
    EntityList list;
    entity_list_init(&list);
    entity_add(&list, entity_spawn(ENTITY_PROP, vec3(0, 0, -1), ""));

    /* Props have no touch/use hooks: nothing reacts */
    int found[4];
    Cylinder body = {.base = vec3(0, 0, -1), .radius = COLLECT_RADIUS, .height = 1.6f};
    ASSERT(entity_overlap(&list, body, found, 4) == 0);
    ASSERT(entity_touch(&list, 0) == TOUCH_IGNORED);
    ASSERT_EQ_INT(entity_find_usable(&list, vec3(0, 0, 0), vec3(0, 0, -1), USE_RANGE),
                  ENTITY_INDEX_NONE);
    ASSERT(list.items[0].active);

    entity_list_free(&list);
}

int main(void) {
    TEST_SUITE("entity");
    RUN_TEST(test_entity_list_init);
    RUN_TEST(test_entity_add);
    RUN_TEST(test_entity_add_grows);
    RUN_TEST(test_entity_meshes_init);
    RUN_TEST(test_entity_collect_pickup);
    RUN_TEST(test_entity_door_usable);
    RUN_TEST(test_entity_door_shape);
    RUN_TEST(test_entity_build_triangles);
    RUN_TEST(test_entity_inactive_skipped);
    RUN_TEST(test_entity_type_by_name);
    RUN_TEST(test_entity_spawn_reads_args);
    RUN_TEST(test_entity_hookless_type_is_inert);
    TEST_REPORT();
}
