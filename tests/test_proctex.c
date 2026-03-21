#include "test_framework.h"
#include "../src/proctex.h"

TEST(test_proctex_find_concrete) {
    ASSERT_EQ_INT(proctex_find("concrete"), PTEX_CONCRETE);
}

TEST(test_proctex_find_all_names) {
    ASSERT_EQ_INT(proctex_find("concrete"), PTEX_CONCRETE);
    ASSERT_EQ_INT(proctex_find("brick"), PTEX_BRICK);
    ASSERT_EQ_INT(proctex_find("metal_panel"), PTEX_METAL_PANEL);
    ASSERT_EQ_INT(proctex_find("metal_grate"), PTEX_METAL_GRATE);
    ASSERT_EQ_INT(proctex_find("rust"), PTEX_RUST);
    ASSERT_EQ_INT(proctex_find("floor_tile"), PTEX_FLOOR_TILE);
    ASSERT_EQ_INT(proctex_find("pipe"), PTEX_PIPE);
}

TEST(test_proctex_find_none) {
    ASSERT_EQ_INT(proctex_find("none"), -1);
}

TEST(test_proctex_find_null) {
    ASSERT_EQ_INT(proctex_find(NULL), -1);
}

TEST(test_proctex_find_unknown) {
    ASSERT_EQ_INT(proctex_find("bogus"), -1);
}

TEST(test_proctex_generate_concrete) {
    Texture tex = {0};
    proctex_generate(&tex, PTEX_CONCRETE);
    ASSERT(tex.pixels != NULL);
    ASSERT_EQ_INT(tex.width, 32);
    ASSERT_EQ_INT(tex.height, 32);
    texture_free(&tex);
}

TEST(test_proctex_generate_brick) {
    Texture tex = {0};
    proctex_generate(&tex, PTEX_BRICK);
    ASSERT(tex.pixels != NULL);
    ASSERT_EQ_INT(tex.width, 64);
    ASSERT_EQ_INT(tex.height, 64);
    texture_free(&tex);
}

TEST(test_proctex_generate_all) {
    Texture textures[PTEX_COUNT];
    proctex_generate_all(textures);
    for (int i = 0; i < PTEX_COUNT; i++) {
        ASSERT(textures[i].pixels != NULL);
        ASSERT(textures[i].width > 0);
        ASSERT(textures[i].height > 0);
    }
    for (int i = 0; i < PTEX_COUNT; i++)
        texture_free(&textures[i]);
}

TEST(test_proctex_sample) {
    Texture tex = {0};
    proctex_generate(&tex, PTEX_CONCRETE);
    Vec3 col = texture_sample(&tex, 0.5f, 0.5f);
    ASSERT(col.x >= 0 && col.x <= 1);
    ASSERT(col.y >= 0 && col.y <= 1);
    ASSERT(col.z >= 0 && col.z <= 1);
    texture_free(&tex);
}

TEST(test_proctex_nonzero_content) {
    Texture tex = {0};
    proctex_generate(&tex, PTEX_METAL_PANEL);
    int nonzero = 0;
    for (int i = 0; i < tex.width * tex.height * 3; i++) {
        if (tex.pixels[i] != 0) { nonzero = 1; break; }
    }
    ASSERT(nonzero);
    texture_free(&tex);
}

int main(void) {
    TEST_SUITE("proctex");
    RUN_TEST(test_proctex_find_concrete);
    RUN_TEST(test_proctex_find_all_names);
    RUN_TEST(test_proctex_find_none);
    RUN_TEST(test_proctex_find_null);
    RUN_TEST(test_proctex_find_unknown);
    RUN_TEST(test_proctex_generate_concrete);
    RUN_TEST(test_proctex_generate_brick);
    RUN_TEST(test_proctex_generate_all);
    RUN_TEST(test_proctex_sample);
    RUN_TEST(test_proctex_nonzero_content);
    TEST_REPORT();
}
