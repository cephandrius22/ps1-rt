#include "test_framework.h"
#include "overlay.h"

#define BG 0xFF808080u /* mid grey */

static Framebuffer fb; /* 300 KB: keep off the stack */

static int count_changed(int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            n += fb.pixels[y * SCREEN_W + x] != BG;
        }
    }
    return n;
}

TEST(test_overlay_draws_hud_corners) {
    framebuffer_clear(&fb, BG);
    OverlayState s = {.mode = OVERLAY_PLAYING, .health = 100, .ammo = 50};
    overlay_draw(&fb, &s);

    int bottom = SCREEN_H - 24;
    ASSERT(count_changed(0, bottom, SCREEN_W / 3, SCREEN_H) > 0);            /* health */
    ASSERT(count_changed(SCREEN_W * 2 / 3, bottom, SCREEN_W, SCREEN_H) > 0); /* ammo */
    ASSERT(count_changed(0, 0, SCREEN_W, bottom) == 0);                      /* view clear */
}

TEST(test_overlay_dead_tints_red) {
    framebuffer_clear(&fb, BG);
    OverlayState s = {.mode = OVERLAY_DEAD, .health = 0, .ammo = 0};
    overlay_draw(&fb, &s);

    uint32_t p = fb.pixels[(SCREEN_H / 2) * SCREEN_W + SCREEN_W / 2];
    uint32_t r = (p >> 16) & 0xFF;
    uint32_t g = (p >> 8) & 0xFF;
    uint32_t b = p & 0xFF;
    ASSERT(r == 0x80);
    ASSERT(g < r && b < r);
}

int main(void) {
    TEST_SUITE("overlay");
    RUN_TEST(test_overlay_draws_hud_corners);
    RUN_TEST(test_overlay_dead_tints_red);
    TEST_REPORT();
}
