#include "test_framework.h"
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
    ASSERT(em.initialized[ENTITY_DOOR]);
    ASSERT(em.initialized[ENTITY_PROP]);
    ASSERT(em.meshes[ENTITY_PICKUP_HEALTH].count > 0);
    ASSERT(em.meshes[ENTITY_DOOR].count > 0);

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

TEST(test_entity_door_toggle) {
    EntityList list;
    entity_list_init(&list);

    Entity door = {
        .type = ENTITY_DOOR, .position = vec3(0, 0, -2),
        .active = true, .door = {.angle = 0, .target_angle = 0,
        .swing_speed = DOOR_SWING_SPEED, .hinge = vec3(0, 0, -2)}
    };
    entity_add(&list, door);

    /* Player facing the door, within USE_RANGE */
    Vec3 player_pos = vec3(0, 1, 0);
    Vec3 forward = vec3(0, 0, -1);

    int used = ENTITY_INDEX_NONE;
    ASSERT(entity_try_use(&list, player_pos, forward, USE_RANGE, &used) == USE_OPENED);
    ASSERT(used == 0);
    ASSERT(list.items[0].door.target_angle > 0);

    /* Toggle back */
    ASSERT(entity_try_use(&list, player_pos, forward, USE_RANGE, NULL) == USE_CLOSED);
    ASSERT_NEAR(list.items[0].door.target_angle, 0, 1e-6f);

    entity_list_free(&list);
}

TEST(test_entity_door_out_of_range) {
    EntityList list;
    entity_list_init(&list);

    Entity door = {
        .type = ENTITY_DOOR, .position = vec3(0, 0, -10),
        .active = true, .door = {.angle = 0, .target_angle = 0,
        .swing_speed = DOOR_SWING_SPEED, .hinge = vec3(0, 0, -10)}
    };
    entity_add(&list, door);

    Vec3 player_pos = vec3(0, 1, 0);
    Vec3 forward = vec3(0, 0, -1);

    ASSERT(entity_try_use(&list, player_pos, forward, USE_RANGE, NULL) == USE_NOTHING);
    ASSERT_NEAR(list.items[0].door.target_angle, 0, 1e-6f);

    entity_list_free(&list);
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

TEST(test_entity_update_door_animates) {
    EntityList list;
    entity_list_init(&list);

    Entity door = {
        .type = ENTITY_DOOR, .active = true,
        .door = {.angle = 0, .target_angle = 1.5f, .swing_speed = DOOR_SWING_SPEED,
                 .hinge = vec3(0, 0, 0)}
    };
    entity_add(&list, door);

    /* Simulate several frames */
    for (int i = 0; i < 60; i++) {
        entity_update(&list, 1.0f / 60.0f, (float)i / 60.0f);
    }

    ASSERT(list.items[0].door.angle > 0);
    ASSERT(list.items[0].door.angle <= 1.5f);

    entity_list_free(&list);
}

TEST(test_entity_type_by_name) {
    ASSERT_EQ_INT(entity_type_by_name("health"), ENTITY_PICKUP_HEALTH);
    ASSERT_EQ_INT(entity_type_by_name("ammo"), ENTITY_PICKUP_AMMO);
    ASSERT_EQ_INT(entity_type_by_name("door"), ENTITY_DOOR);
    ASSERT_EQ_INT(entity_type_by_name("prop"), ENTITY_PROP);
    ASSERT_EQ_INT(entity_type_by_name("dragon"), ENTITY_NONE);
}

TEST(test_entity_spawn_reads_args) {
    Entity hp = entity_spawn(ENTITY_PICKUP_HEALTH, vec3(1, 2, 3), "amount=40 mat=5");
    ASSERT(hp.active);
    ASSERT_NEAR(hp.position.y, 2.0f, 1e-6f);
    ASSERT_NEAR(hp.pickup.amount, 40.0f, 1e-6f);
    ASSERT_EQ_INT(hp.material_id, 5);

    /* Defaults when keys are absent; hinge falls back to position */
    Entity door = entity_spawn(ENTITY_DOOR, vec3(4, 0, -6), "");
    ASSERT_NEAR(door.door.swing_speed, DOOR_SWING_SPEED, 1e-6f);
    ASSERT_NEAR(door.door.hinge.x, 4.0f, 1e-6f);
    ASSERT_NEAR(door.door.hinge.z, -6.0f, 1e-6f);
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
    ASSERT(entity_try_use(&list, vec3(0, 0, 0), vec3(0, 0, -1), USE_RANGE, NULL) == USE_NOTHING);
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
    RUN_TEST(test_entity_door_toggle);
    RUN_TEST(test_entity_door_out_of_range);
    RUN_TEST(test_entity_build_triangles);
    RUN_TEST(test_entity_inactive_skipped);
    RUN_TEST(test_entity_update_door_animates);
    RUN_TEST(test_entity_type_by_name);
    RUN_TEST(test_entity_spawn_reads_args);
    RUN_TEST(test_entity_hookless_type_is_inert);
    TEST_REPORT();
}
