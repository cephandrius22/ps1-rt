#include "test_framework.h"
#include "../src/texture.h"
#include <stdlib.h>

static Texture make_test_texture(int w, int h) {
    Texture tex;
    tex.width = w;
    tex.height = h;
    tex.pixels = malloc(w * h * 3);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t *p = tex.pixels + (y * w + x) * 3;
            p[0] = (uint8_t)(x * 255 / (w - 1));  // R = horizontal gradient
            p[1] = (uint8_t)(y * 255 / (h - 1));  // G = vertical gradient
            p[2] = 128;                            // B = constant
        }
    }
    return tex;
}

TEST(test_texture_sample_origin) {
    Texture tex = make_test_texture(4, 4);
    Vec3 col = texture_sample(&tex, 0.0f, 0.0f);
    // Top-left pixel: R=0, G=0, B=128
    ASSERT_NEAR(col.x, 0.0f, 0.01f);
    ASSERT_NEAR(col.y, 0.0f, 0.01f);
    ASSERT_NEAR(col.z, 128.0f / 255.0f, 0.01f);
    texture_free(&tex);
}

TEST(test_texture_sample_center) {
    Texture tex = make_test_texture(4, 4);
    Vec3 col = texture_sample(&tex, 0.5f, 0.5f);
    // Should be somewhere in the middle of the gradient
    ASSERT(col.x > 0.2f && col.x < 0.8f);
    ASSERT(col.y > 0.2f && col.y < 0.8f);
    texture_free(&tex);
}

TEST(test_texture_sample_wrap) {
    Texture tex = make_test_texture(4, 4);
    Vec3 col1 = texture_sample(&tex, 0.25f, 0.25f);
    Vec3 col2 = texture_sample(&tex, 1.25f, 1.25f);
    // Should wrap and give same result
    ASSERT_NEAR(col1.x, col2.x, 0.01f);
    ASSERT_NEAR(col1.y, col2.y, 0.01f);
    ASSERT_NEAR(col1.z, col2.z, 0.01f);
    texture_free(&tex);
}

TEST(test_texture_sample_negative_uv) {
    Texture tex = make_test_texture(4, 4);
    Vec3 col = texture_sample(&tex, -0.25f, -0.25f);
    // Should wrap to positive and not crash
    ASSERT(col.z > 0.4f);  // B channel is constant 128
    texture_free(&tex);
}

TEST(test_texture_sample_null_pixels) {
    Texture tex = {.pixels = NULL, .width = 4, .height = 4};
    Vec3 col = texture_sample(&tex, 0.5f, 0.5f);
    // Should return magenta (1, 0, 1)
    ASSERT_NEAR(col.x, 1.0f, 1e-6f);
    ASSERT_NEAR(col.y, 0.0f, 1e-6f);
    ASSERT_NEAR(col.z, 1.0f, 1e-6f);
}

TEST(test_texture_free) {
    Texture tex = make_test_texture(4, 4);
    ASSERT(tex.pixels != NULL);
    texture_free(&tex);
    ASSERT(tex.pixels == NULL);
}

int main(void) {
    TEST_SUITE("texture sampling");
    RUN_TEST(test_texture_sample_origin);
    RUN_TEST(test_texture_sample_center);
    RUN_TEST(test_texture_sample_wrap);
    RUN_TEST(test_texture_sample_negative_uv);
    RUN_TEST(test_texture_sample_null_pixels);
    RUN_TEST(test_texture_free);
    TEST_REPORT();
}
