#include "test_framework.h"
#include "level.h"

TEST(test_level_load_success) {
    Scene scene;
    scene_init(&scene);
    LevelInfo info;
    int result = level_load(&scene, "tests/test.level", &info);
    ASSERT_EQ_INT(result, 0);
    scene_free(&scene);
}

TEST(test_level_spawn) {
    Scene scene;
    scene_init(&scene);
    LevelInfo info;
    level_load(&scene, "tests/test.level", &info);
    ASSERT_NEAR(info.spawn_pos.x, 5.0f, 0.01f);
    ASSERT_NEAR(info.spawn_pos.y, 0.0f, 0.01f);
    ASSERT_NEAR(info.spawn_pos.z, 10.0f, 0.01f);
    ASSERT_NEAR(info.spawn_yaw, 90.0f, 0.01f);
    scene_free(&scene);
}

TEST(test_level_materials) {
    Scene scene;
    scene_init(&scene);
    LevelInfo info;
    level_load(&scene, "tests/test.level", &info);
    /* 4 declared (stone, floor, ceil, red_flat) + the decor's default
     * "barrel" + "projectiles", prepared for runtime spawns */
    ASSERT_EQ_INT(scene.material_count, 6);
    /* stone has concrete texture */
    ASSERT(scene.materials[0].texture_id >= 0);
    /* ceil has no texture */
    ASSERT_EQ_INT(scene.materials[2].texture_id, -1);
    /* red_flat has no texture */
    ASSERT_EQ_INT(scene.materials[3].texture_id, -1);
    ASSERT_NEAR(scene.materials[3].color.x, 0.9f, 0.01f);
    scene_free(&scene);
}

TEST(test_level_textures) {
    Scene scene;
    scene_init(&scene);
    LevelInfo info;
    level_load(&scene, "tests/test.level", &info);
    /* 4 procedural textures: concrete, floor_tile, barrel (decor default),
     * projectiles (runtime spawns) */
    ASSERT_EQ_INT(scene.texture_count, 4);
    ASSERT(scene.textures[0].pixels != NULL);
    ASSERT(scene.textures[1].pixels != NULL);
    scene_free(&scene);
}

TEST(test_level_geometry) {
    Scene scene;
    scene_init(&scene);
    LevelInfo info;
    level_load(&scene, "tests/test.level", &info);
    /* room = 6 faces * 2 tris = 12, box = 6 faces * 2 tris = 12, total = 24 */
    ASSERT(scene.static_bvh.tri_count >= 24);
    scene_free(&scene);
}

TEST(test_level_entities) {
    Scene scene;
    scene_init(&scene);
    LevelInfo info;
    level_load(&scene, "tests/test.level", &info);
    ASSERT_EQ_INT(scene.entities.count, 3);
    /* First entity is health pickup */
    ASSERT_EQ_INT(scene.entities.items[0].type, ENTITY_PICKUP_HEALTH);
    ASSERT_NEAR(scene.entities.items[0].pickup.amount, 50.0f, 0.01f);
    ASSERT_EQ_INT(scene.entities.items[1].type, ENTITY_DECOR);
    ASSERT_EQ_INT(scene.entities.items[2].type, ENTITY_DOOR);
    scene_free(&scene);
}

/* brush directive: entity at the box center, faces kept as its model */
TEST(test_level_brush_door) {
    Scene scene;
    scene_init(&scene);
    LevelInfo info;
    level_load(&scene, "tests/test.level", &info);

    const Entity *door = &scene.entities.items[2];
    ASSERT_NEAR(door->position.y, 1.25f, 1e-4f);
    ASSERT_NEAR(door->position.z, -4.9f, 1e-4f);
    ASSERT_NEAR(door->half_size.x, 1.0f, 1e-4f);
    ASSERT(door->model > 0);
    ASSERT_EQ_INT(scene.entity_meshes.models[door->model - 1].count, 12);
    ASSERT_NEAR(door->mover.dir.y, 1.0f, 1e-6f);   /* rises by default */
    ASSERT(door->mover.travel > 2.0f && door->mover.travel < 2.5f);
    ASSERT_NEAR(door->mover.wait, 2.0f, 1e-6f);
    scene_free(&scene);
}

TEST(test_level_lights) {
    Scene scene;
    scene_init(&scene);
    LevelInfo info;
    level_load(&scene, "tests/test.level", &info);
    ASSERT_EQ_INT(scene.lights.count, 2);
    /* First light has shadows */
    ASSERT(scene.lights.items[0].cast_shadows);
    ASSERT_NEAR(scene.lights.items[0].intensity, 3.0f, 0.01f);
    /* Second light has flicker */
    ASSERT_NEAR(scene.lights.items[1].flicker_speed, 3.0f, 0.01f);
    ASSERT_NEAR(scene.lights.items[1].flicker_amount, 0.4f, 0.01f);
    scene_free(&scene);
}

TEST(test_level_missing_file) {
    Scene scene;
    scene_init(&scene);
    LevelInfo info;
    int result = level_load(&scene, "nonexistent.level", &info);
    ASSERT_EQ_INT(result, -1);
    scene_free(&scene);
}

/* mat=<name> picks a declared material; without mat= the type's default
 * material is created (decor → "barrel", textured) */
TEST(test_level_entity_materials) {
    Scene scene;
    scene_init(&scene);
    LevelInfo info;
    level_load(&scene, "tests/test.level", &info);

    const Entity *health = &scene.entities.items[0];
    const Entity *decor = &scene.entities.items[1];
    ASSERT_EQ_INT(health->material_id, 3); /* red_flat */
    ASSERT(decor->material_id != health->material_id);
    ASSERT(scene.materials[decor->material_id].texture_id >= 0);
    scene_free(&scene);
}

int main(void) {
    TEST_SUITE("level");
    RUN_TEST(test_level_load_success);
    RUN_TEST(test_level_spawn);
    RUN_TEST(test_level_materials);
    RUN_TEST(test_level_textures);
    RUN_TEST(test_level_geometry);
    RUN_TEST(test_level_entities);
    RUN_TEST(test_level_brush_door);
    RUN_TEST(test_level_lights);
    RUN_TEST(test_level_missing_file);
    RUN_TEST(test_level_entity_materials);
    TEST_REPORT();
}
