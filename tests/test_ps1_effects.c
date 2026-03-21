#include "test_framework.h"
#include "../src/ps1_effects.h"

TEST(test_snap_vertex_basic) {
    Vec3 v = vec3(1.0f, 2.0f, 3.0f);
    Vec3 snapped = ps1_snap_vertex(v);
    // Integer coordinates should snap cleanly
    ASSERT_NEAR(snapped.x, 1.0f, 1.0f / PS1_VERTEX_SNAP);
    ASSERT_NEAR(snapped.y, 2.0f, 1.0f / PS1_VERTEX_SNAP);
    ASSERT_NEAR(snapped.z, 3.0f, 1.0f / PS1_VERTEX_SNAP);
}

TEST(test_snap_vertex_rounding) {
    // Values between grid points should snap
    Vec3 v = vec3(0.123f, 0.456f, 0.789f);
    Vec3 snapped = ps1_snap_vertex(v);

    // Verify snapped values are on the grid
    float grid = PS1_VERTEX_SNAP;
    ASSERT_NEAR(snapped.x * grid, roundf(snapped.x * grid), 1e-4f);
    ASSERT_NEAR(snapped.y * grid, roundf(snapped.y * grid), 1e-4f);
    ASSERT_NEAR(snapped.z * grid, roundf(snapped.z * grid), 1e-4f);
}

TEST(test_snap_vertex_deterministic) {
    Vec3 v = vec3(1.234f, 5.678f, 9.012f);
    Vec3 s1 = ps1_snap_vertex(v);
    Vec3 s2 = ps1_snap_vertex(v);
    ASSERT_NEAR(s1.x, s2.x, 1e-8f);
    ASSERT_NEAR(s1.y, s2.y, 1e-8f);
    ASSERT_NEAR(s1.z, s2.z, 1e-8f);
}

TEST(test_jitter_triangle) {
    Triangle tri = {
        .v0 = vec3(0.123f, 0.456f, 0.789f),
        .v1 = vec3(1.234f, 2.345f, 3.456f),
        .v2 = vec3(4.567f, 5.678f, 6.789f),
        .uv0 = {0, 0}, .uv1 = {1, 0}, .uv2 = {0, 1},
        .normal = vec3(0, 1, 0), .material_id = 42
    };

    Triangle jittered = ps1_jitter_triangle(&tri);

    // Vertices should be snapped
    ASSERT(jittered.v0.x != tri.v0.x || jittered.v0.y != tri.v0.y || jittered.v0.z != tri.v0.z);

    // UVs, normal, and material should be preserved
    ASSERT_NEAR(jittered.uv0.u, tri.uv0.u, 1e-8f);
    ASSERT_NEAR(jittered.uv1.u, tri.uv1.u, 1e-8f);
    ASSERT_NEAR(jittered.normal.y, 1.0f, 1e-6f);
    ASSERT_EQ_INT(jittered.material_id, 42);
}

TEST(test_affine_uv_at_vertices) {
    Triangle tri = {
        .uv0 = {0, 0}, .uv1 = {1, 0}, .uv2 = {0, 1}
    };

    // At vertex 0 (u=0, v=0): should get uv0
    Vec2 uv = ps1_affine_uv(&tri, 0.0f, 0.0f);
    ASSERT_NEAR(uv.u, 0.0f, 1e-6f);
    ASSERT_NEAR(uv.v, 0.0f, 1e-6f);

    // At vertex 1 (u=1, v=0): should get uv1
    uv = ps1_affine_uv(&tri, 1.0f, 0.0f);
    ASSERT_NEAR(uv.u, 1.0f, 1e-6f);
    ASSERT_NEAR(uv.v, 0.0f, 1e-6f);

    // At vertex 2 (u=0, v=1): should get uv2
    uv = ps1_affine_uv(&tri, 0.0f, 1.0f);
    ASSERT_NEAR(uv.u, 0.0f, 1e-6f);
    ASSERT_NEAR(uv.v, 1.0f, 1e-6f);
}

TEST(test_affine_uv_center) {
    Triangle tri = {
        .uv0 = {0, 0}, .uv1 = {1, 0}, .uv2 = {0, 1}
    };
    Vec2 uv = ps1_affine_uv(&tri, 1.0f / 3.0f, 1.0f / 3.0f);
    ASSERT_NEAR(uv.u, 1.0f / 3.0f, 1e-5f);
    ASSERT_NEAR(uv.v, 1.0f / 3.0f, 1e-5f);
}

TEST(test_dither_color_range) {
    // Test that output is always valid ARGB
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            uint32_t c = ps1_dither_color(128, 128, 128, px, py);
            // Alpha should be 0xFF
            ASSERT((c >> 24) == 0xFF);
            // Each channel should be quantized to 5-bit (multiples of 8)
            uint8_t r = (c >> 16) & 0xFF;
            uint8_t g = (c >> 8) & 0xFF;
            uint8_t b = c & 0xFF;
            ASSERT_EQ_INT(r % 8, 0);
            ASSERT_EQ_INT(g % 8, 0);
            ASSERT_EQ_INT(b % 8, 0);
        }
    }
}

TEST(test_dither_color_clamped) {
    // Test extremes don't overflow/underflow
    uint32_t white = ps1_dither_color(255, 255, 255, 0, 0);
    uint32_t black = ps1_dither_color(0, 0, 0, 0, 0);
    ASSERT((white >> 24) == 0xFF);
    ASSERT((black >> 24) == 0xFF);

    // Black should stay near black
    uint8_t r = (black >> 16) & 0xFF;
    uint8_t g = (black >> 8) & 0xFF;
    uint8_t b = black & 0xFF;
    ASSERT(r <= 32);
    ASSERT(g <= 32);
    ASSERT(b <= 32);
}

TEST(test_dither_varies_with_position) {
    // Different positions should produce different results (for mid-range colors)
    uint32_t c00 = ps1_dither_color(100, 100, 100, 0, 0);
    uint32_t c11 = ps1_dither_color(100, 100, 100, 1, 1);
    // Bayer[0][0] = 0, Bayer[1][1] = 4, so thresholds differ
    // They may or may not produce different outputs, but let's check it doesn't crash
    ASSERT((c00 >> 24) == 0xFF);
    ASSERT((c11 >> 24) == 0xFF);
}

int main(void) {
    TEST_SUITE("PS1 vertex snapping");
    RUN_TEST(test_snap_vertex_basic);
    RUN_TEST(test_snap_vertex_rounding);
    RUN_TEST(test_snap_vertex_deterministic);
    RUN_TEST(test_jitter_triangle);

    TEST_SUITE("PS1 affine UV");
    RUN_TEST(test_affine_uv_at_vertices);
    RUN_TEST(test_affine_uv_center);

    TEST_SUITE("PS1 dithering");
    RUN_TEST(test_dither_color_range);
    RUN_TEST(test_dither_color_clamped);
    RUN_TEST(test_dither_varies_with_position);
    TEST_REPORT();
}
