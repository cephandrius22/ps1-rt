#include "test_framework.h"
#include "screenfx.h"

TEST(test_screenfx_decays_to_zero) {
    ScreenFx fx = {0};
    screenfx_shake(&fx, 0.05f);
    screenfx_flash(&fx, vec3(1, 0, 0), 0.8f);
    for (int i = 0; i < 120; i++) {
        screenfx_update(&fx, 1.0f / 60.0f);
    }
    ASSERT_NEAR(fx.shake, 0.0f, 1e-6f);
    ASSERT_NEAR(fx.flash, 0.0f, 1e-6f);
}

TEST(test_screenfx_keeps_strongest) {
    ScreenFx fx = {0};
    screenfx_shake(&fx, 0.05f);
    screenfx_shake(&fx, 0.01f);
    ASSERT_NEAR(fx.shake, 0.05f, 1e-6f);

    screenfx_flash(&fx, vec3(1, 0, 0), 0.6f);
    screenfx_flash(&fx, vec3(0, 1, 0), 0.2f); /* weaker: ignored */
    ASSERT_NEAR(fx.flash, 0.6f, 1e-6f);
    ASSERT_NEAR(fx.flash_color.x, 1.0f, 1e-6f);

    screenfx_flash(&fx, vec3(0, 0, 1), 5.0f); /* clamped to 1 */
    ASSERT_NEAR(fx.flash, 1.0f, 1e-6f);
}

TEST(test_screenfx_offset) {
    ScreenFx fx = {0};
    float yaw, pitch;
    screenfx_camera_offset(&fx, 1.23f, &yaw, &pitch);
    ASSERT_NEAR(yaw, 0.0f, 1e-9f);
    ASSERT_NEAR(pitch, 0.0f, 1e-9f);

    /* Shaking: offsets bounded by the strength, and they move over time */
    screenfx_shake(&fx, 0.02f);
    float yaw2, pitch2;
    screenfx_camera_offset(&fx, 1.23f, &yaw, &pitch);
    screenfx_camera_offset(&fx, 1.30f, &yaw2, &pitch2);
    ASSERT(fabsf(yaw) <= 0.02f + 1e-6f && fabsf(pitch) <= 0.02f + 1e-6f);
    ASSERT(yaw != yaw2 || pitch != pitch2);
}

int main(void) {
    TEST_SUITE("screenfx");
    RUN_TEST(test_screenfx_decays_to_zero);
    RUN_TEST(test_screenfx_keeps_strongest);
    RUN_TEST(test_screenfx_offset);
    TEST_REPORT();
}
