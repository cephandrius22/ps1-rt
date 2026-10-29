#include "test_framework.h"
#include "level.h"
#include "render.h"
#include <stdio.h>

#define LEVEL_TMP "build/tests/materials_tmp.level"

static Scene s;
static Framebuffer fb; /* 300 KB: keep off the stack */

/* Load a scratch level from text into s */
static void load(const char *text) {
    FILE *f = fopen(LEVEL_TMP, "w");
    fputs(text, f);
    fclose(f);
    scene_init(&s);
    LevelInfo info;
    level_load(&s, LEVEL_TMP, &info);
    remove(LEVEL_TMP);
}

/* Brightest channel across a short column of pixels at x */
static int column_peak(int x) {
    int peak = 0;
    for (int y = SCREEN_H / 2 - 10; y < SCREEN_H / 2 + 10; y++) {
        uint32_t p = fb.pixels[y * SCREEN_W + x];
        for (int shift = 0; shift <= 16; shift += 8) {
            int c = (int)((p >> shift) & 0xFF);
            peak = c > peak ? c : peak;
        }
    }
    return peak;
}

/* Animated textures bring their motion; material keys override it */
TEST(test_material_motion_defaults_and_keys) {
    load("material ooze slime 1 1 1\nmaterial tv screen 1 1 1 glow=0.2 pulse=0\n"
         "material puddle water 1 1 1 scroll_u=0.5\nmaterial wall concrete 1 1 1\n");
    const Material *ooze = &s.materials[0];
    const Material *tv = &s.materials[1];
    const Material *puddle = &s.materials[2];
    const Material *wall = &s.materials[3];

    ASSERT(ooze->scroll.u > 0 && ooze->glow > 0);
    ASSERT_NEAR(tv->glow, 0.2f, 1e-6f);
    ASSERT_NEAR(tv->pulse, 0.0f, 1e-6f);
    ASSERT_NEAR(puddle->scroll.u, 0.5f, 1e-6f);
    ASSERT(puddle->scroll.v > 0);                 /* untouched default */
    ASSERT(wall->glow == 0 && wall->scroll.u == 0);
    scene_free(&s);
}

/* No lights, no ambient: a glowing wall still shows, a plain one doesn't */
TEST(test_material_glow_in_the_dark) {
    load("ambient 0\nsun 0 1 0 1 1 1 0\nfog 100 200 0 0 0\n"
         "material tv screen 1 1 1 pulse=0\nmaterial wall concrete 1 1 1\n"
         "quad -2 -1 -3  0 -1 -3  0 1 -3  -2 1 -3 tv\n"
         "quad 0 -1 -3  2 -1 -3  2 1 -3  0 1 -3 wall\n");
    Camera cam = camera_create(vec3(0, 0, 0), 0, 0, 1.2f);
    render_scene_lit(&fb, &cam, &s, 0, NULL);

    /* Dither adds up to a few levels of noise even to black */
    ASSERT(column_peak(SCREEN_W / 2 - 40) > 60);
    ASSERT(column_peak(SCREEN_W / 2 + 40) < 20);
    scene_free(&s);
}

#define TGA_TMP "build/tests/materials_wall.tga"

/* A material naming a .tga path uses that file, at its own size */
TEST(test_material_tga_file) {
    uint8_t pixels[8 * 4 * 3];
    for (int i = 0; i < 8 * 4 * 3; i++) {
        pixels[i] = (uint8_t)(i * 7);
    }
    Texture src = {.pixels = pixels, .width = 8, .height = 4};
    ASSERT_EQ_INT(texture_save_tga(&src, TGA_TMP), 0);

    load("material wall " TGA_TMP " 1 1 1\nmaterial ghost no_such_texture 0.5 0.5 0.5\n");
    remove(TGA_TMP);
    const Material *wall = &s.materials[0];
    ASSERT(wall->texture_id >= 0);
    ASSERT_EQ_INT(s.textures[wall->texture_id].width, 8);
    ASSERT_EQ_INT(s.textures[wall->texture_id].height, 4);
    ASSERT_EQ_INT(s.materials[1].texture_id, -1);   /* unknown: flat color */
    scene_free(&s);
}

int main(void) {
    TEST_SUITE("materials");
    RUN_TEST(test_material_motion_defaults_and_keys);
    RUN_TEST(test_material_glow_in_the_dark);
    RUN_TEST(test_material_tga_file);
    TEST_REPORT();
}
