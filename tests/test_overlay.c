#include "test_framework.h"
#include "overlay.h"
#include <stdio.h>
#include <stdlib.h>

#define BG 0xFF808080u /* mid grey */

static Framebuffer fb; /* 300 KB: keep off the stack */
static Overlay ov;    /* viewmodel texture cache */

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
    overlay_draw(&ov, &fb, &s);

    int bottom = SCREEN_H - 24;
    ASSERT(count_changed(0, bottom, SCREEN_W / 3, SCREEN_H) > 0);            /* health */
    ASSERT(count_changed(SCREEN_W * 2 / 3, bottom, SCREEN_W, SCREEN_H) > 0); /* ammo */
    ASSERT(count_changed(0, 0, SCREEN_W, bottom) == 0);                      /* view clear */
}

TEST(test_overlay_dead_tints_red) {
    framebuffer_clear(&fb, BG);
    OverlayState s = {.mode = OVERLAY_DEAD, .health = 0, .ammo = 0};
    overlay_draw(&ov, &fb, &s);

    uint32_t p = fb.pixels[(SCREEN_H / 2) * SCREEN_W + SCREEN_W / 2];
    uint32_t r = (p >> 16) & 0xFF;
    uint32_t g = (p >> 8) & 0xFF;
    uint32_t b = p & 0xFF;
    ASSERT(r == 0x80);
    ASSERT(g < r && b < r);
}

TEST(test_overlay_level_end_shows_kills) {
    framebuffer_clear(&fb, BG);
    OverlayState s = {.mode = OVERLAY_LEVEL_END, .health = 50, .ammo = 9,
                      .kills = 3, .kills_total = 4};
    overlay_draw(&ov, &fb, &s);

    /* Frame dimmed; something bright drawn in the middle band */
    uint32_t corner = fb.pixels[10 * SCREEN_W + 10];
    ASSERT(((corner >> 16) & 0xFF) < 0x80);
    int bright = 0;
    for (int y = SCREEN_H / 2 - 20; y < SCREEN_H / 2 + 20; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            bright += ((fb.pixels[y * SCREEN_W + x] >> 16) & 0xFF) > 0xC0;
        }
    }
    ASSERT(bright > 0);
}

TEST(test_overlay_debug_text_top_left) {
    framebuffer_clear(&fb, BG);
    OverlayState s = {.mode = OVERLAY_PLAYING, .health = 100, .ammo = 50, .debug_count = 2};
    snprintf(s.debug[0].text, OVERLAY_DEBUG_CHARS, "7.1 MS 60 FPS");
    snprintf(s.debug[1].text, OVERLAY_DEBUG_CHARS, "TRI 252/30000");
    s.debug[1].severity = DEBUG_OVER;
    overlay_draw(&ov, &fb, &s);

    ASSERT(count_changed(0, 0, 80, 20) > 0);                       /* text drawn */
    ASSERT(count_changed(SCREEN_W / 2, 0, SCREEN_W, SCREEN_H / 2) == 0);

    /* Hidden when there are no lines */
    framebuffer_clear(&fb, BG);
    s.debug_count = 0;
    overlay_draw(&ov, &fb, &s);
    ASSERT(count_changed(0, 0, 80, 20) == 0);
}

/* Pistol viewmodel lands bottom-center; light scales its pixels */
TEST(test_overlay_viewmodel) {
    framebuffer_clear(&fb, BG);
    OverlayState s = {.mode = OVERLAY_PLAYING,
                      .viewmodel = {.texture = "view_pistol", .light = 1.0f}};
    overlay_draw(&ov, &fb, &s);

    int bottom = SCREEN_H - VIEW_FRAME_H * 2;
    ASSERT(count_changed(SCREEN_W / 2 - 40, bottom, SCREEN_W / 2 + 40, SCREEN_H) > 0);
    ASSERT(count_changed(0, 0, SCREEN_W, bottom - 1) == 0);
    ASSERT(count_changed(0, bottom, SCREEN_W / 2 - 64, SCREEN_H - 20) == 0); /* holes stay */

    /* Same pixel at half light is darker */
    int probe = (SCREEN_H - 30) * SCREEN_W + SCREEN_W / 2;
    uint32_t bright = fb.pixels[probe] & 0xFF;
    framebuffer_clear(&fb, BG);
    s.viewmodel.light = 0.5f;
    overlay_draw(&ov, &fb, &s);
    ASSERT((fb.pixels[probe] & 0xFF) < bright);

    /* Lowered out of view: nothing drawn */
    framebuffer_clear(&fb, BG);
    s.viewmodel.offset_y = SCREEN_H;
    overlay_draw(&ov, &fb, &s);
    ASSERT(count_changed(0, 0, SCREEN_W, SCREEN_H - 20) == 0);
}

/* Message: centered text near the top; keycards: colored icons bottom right */
TEST(test_overlay_message_and_keycards) {
    framebuffer_clear(&fb, BG);
    OverlayState s = {.mode = OVERLAY_PLAYING, .message = "You need the red keycard",
                      .keycard_count = 1, .keycard_colors = {{1, 0, 0}}};
    overlay_draw(&ov, &fb, &s);

    ASSERT(count_changed(SCREEN_W / 3, 20, SCREEN_W * 2 / 3, 45) > 0);
    ASSERT(count_changed(0, 20, 20, 45) == 0); /* centered, not left-aligned */

    bool red = false;
    for (int y = SCREEN_H - 40; y < SCREEN_H && !red; y++) {
        for (int x = SCREEN_W - 30; x < SCREEN_W; x++) {
            red |= fb.pixels[y * SCREEN_W + x] == 0xFFFF0000u;
        }
    }
    ASSERT(red);
}

/* Desaturation pulls colored pixels toward grey */
TEST(test_overlay_desaturate) {
    framebuffer_clear(&fb, 0xFFC03020u);
    OverlayState s = {.mode = OVERLAY_PLAYING, .desaturate = 1.0f};
    overlay_draw(&ov, &fb, &s);
    uint32_t p = fb.pixels[(SCREEN_H / 2) * SCREEN_W + SCREEN_W / 2];
    int r = (int)((p >> 16) & 0xFF), g = (int)((p >> 8) & 0xFF), b = (int)(p & 0xFF);
    ASSERT(abs(r - g) <= 1 && abs(g - b) <= 1);
}

/* Battery bar sits above the health number, only when asked for */
TEST(test_overlay_battery_bar) {
    framebuffer_clear(&fb, BG);
    OverlayState hidden = {.mode = OVERLAY_PLAYING, .battery = 0.5f};
    overlay_draw(&ov, &fb, &hidden);
    int bar_rows = count_changed(0, SCREEN_H - 30, 50, SCREEN_H - 20);

    framebuffer_clear(&fb, BG);
    OverlayState shown = {.mode = OVERLAY_PLAYING, .show_battery = true, .battery = 0.5f};
    overlay_draw(&ov, &fb, &shown);
    ASSERT(count_changed(0, SCREEN_H - 30, 50, SCREEN_H - 20) > bar_rows);
}

TEST(test_overlay_flash_tints) {
    framebuffer_clear(&fb, BG);
    OverlayState s = {.mode = OVERLAY_PLAYING, .flash = 1.0f, .flash_color = {1, 0, 0}};
    overlay_draw(&ov, &fb, &s);

    uint32_t p = fb.pixels[(SCREEN_H / 2) * SCREEN_W + SCREEN_W / 2];
    ASSERT(((p >> 16) & 0xFF) > 0x80);  /* pushed toward red */
    ASSERT(((p >> 8) & 0xFF) < 0x80);
}

int main(void) {
    overlay_init(&ov);
    TEST_SUITE("overlay");
    RUN_TEST(test_overlay_draws_hud_corners);
    RUN_TEST(test_overlay_dead_tints_red);
    RUN_TEST(test_overlay_level_end_shows_kills);
    RUN_TEST(test_overlay_debug_text_top_left);
    RUN_TEST(test_overlay_viewmodel);
    RUN_TEST(test_overlay_message_and_keycards);
    RUN_TEST(test_overlay_desaturate);
    RUN_TEST(test_overlay_battery_bar);
    RUN_TEST(test_overlay_flash_tints);
    overlay_free(&ov);
    TEST_REPORT();
}
