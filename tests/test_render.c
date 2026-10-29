#include "test_framework.h"
#include "render.h"

TEST(test_color_rgb) {
    uint32_t c = color_rgb(0xFF, 0x80, 0x40);
    ASSERT((c >> 24) == 0xFF);              // Alpha
    ASSERT(((c >> 16) & 0xFF) == 0xFF);     // R
    ASSERT(((c >> 8) & 0xFF) == 0x80);      // G
    ASSERT((c & 0xFF) == 0x40);             // B
}

TEST(test_color_rgb_black) {
    uint32_t c = color_rgb(0, 0, 0);
    ASSERT(c == 0xFF000000);
}

TEST(test_color_rgb_white) {
    uint32_t c = color_rgb(255, 255, 255);
    ASSERT(c == 0xFFFFFFFF);
}

TEST(test_framebuffer_set_and_read) {
    Framebuffer fb;
    framebuffer_clear(&fb, 0xFF000000);

    framebuffer_set(&fb, 10, 20, 0xFFFF0000);
    ASSERT(fb.pixels[20 * SCREEN_W + 10] == 0xFFFF0000);
}

TEST(test_framebuffer_clear) {
    Framebuffer fb;
    framebuffer_clear(&fb, 0xDEADBEEF);
    ASSERT(fb.pixels[0] == 0xDEADBEEF);
    ASSERT(fb.pixels[SCREEN_W * SCREEN_H - 1] == 0xDEADBEEF);
    ASSERT(fb.pixels[SCREEN_W * SCREEN_H / 2] == 0xDEADBEEF);
}

TEST(test_framebuffer_bounds_check) {
    Framebuffer fb;
    framebuffer_clear(&fb, 0);

    // These should be silently ignored (out of bounds)
    framebuffer_set(&fb, -1, 0, 0xFFFFFFFF);
    framebuffer_set(&fb, SCREEN_W, 0, 0xFFFFFFFF);
    framebuffer_set(&fb, 0, -1, 0xFFFFFFFF);
    framebuffer_set(&fb, 0, SCREEN_H, 0xFFFFFFFF);

    // Verify corners are still zero
    ASSERT(fb.pixels[0] == 0);
    ASSERT(fb.pixels[SCREEN_W - 1] == 0);
    ASSERT(fb.pixels[(SCREEN_H - 1) * SCREEN_W] == 0);
}

int main(void) {
    TEST_SUITE("color");
    RUN_TEST(test_color_rgb);
    RUN_TEST(test_color_rgb_black);
    RUN_TEST(test_color_rgb_white);

    TEST_SUITE("framebuffer");
    RUN_TEST(test_framebuffer_set_and_read);
    RUN_TEST(test_framebuffer_clear);
    RUN_TEST(test_framebuffer_bounds_check);

    TEST_SUITE("sky");
    TEST_REPORT();
}
