#include "test_framework.h"
#include "../src/level.h"

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
    /* 4 materials: stone, floor, ceil, red_flat */
    ASSERT_EQ_INT(scene.material_count, 4);
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
    /* Should have 2 procedural textures: concrete, floor_tile */
    ASSERT_EQ_INT(scene.texture_count, 2);
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
    ASSERT_EQ_INT(scene.entities.count, 2);
    /* First entity is health pickup */
    ASSERT_EQ_INT(scene.entities.items[0].type, ENTITY_PICKUP_HEALTH);
    ASSERT_NEAR(scene.entities.items[0].pickup.amount, 50.0f, 0.01f);
    /* Second entity is door */
    ASSERT_EQ_INT(scene.entities.items[1].type, ENTITY_DOOR);
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

int main(void) {
    TEST_SUITE("level");
    RUN_TEST(test_level_load_success);
    RUN_TEST(test_level_spawn);
    RUN_TEST(test_level_materials);
    RUN_TEST(test_level_textures);
    RUN_TEST(test_level_geometry);
    RUN_TEST(test_level_entities);
    RUN_TEST(test_level_lights);
    RUN_TEST(test_level_missing_file);
    TEST_REPORT();
}
