#include "test_framework.h"
#include "sprite.h"
#include <stdlib.h>

/* 2x1 texture: left texel opaque grey, right texel color key */
static Texture half_keyed(void) {
    Texture t = {.width = 2, .height = 1, .pixels = malloc(6)};
    uint8_t px[6] = {128, 128, 128, TEXTURE_KEY_R, TEXTURE_KEY_G, TEXTURE_KEY_B};
    for (int i = 0; i < 6; i++) {
        t.pixels[i] = px[i];
    }
    return t;
}

/* 1x2 sprite at origin, seen from z=+5 looking down -Z */
static Sprite unit_sprite(void) {
    return (Sprite){.base = vec3(0, 0, 0), .width = 1, .height = 2,
                    .material_id = 3, .frame_count = 1, .owner = 7};
}

static Ray ray_at(float x, float y) {
    return (Ray){vec3(x, y, 5), vec3(0, 0, -1)};
}

TEST(test_sprite_hit_center) {
    Sprite s = unit_sprite();
    HitRecord rec;
    ASSERT(sprite_intersect(&s, NULL, ray_at(0, 1), 0, 100, &rec));
    ASSERT_NEAR(rec.t, 5.0f, 1e-5f);
    ASSERT_NEAR(rec.normal.z, 1.0f, 1e-5f);   /* faces the ray */
    ASSERT_EQ_INT(rec.material_id, 3);
    ASSERT_EQ_INT(rec.owner, 7);
    ASSERT_NEAR(rec.uv.v, 0.5f, 1e-5f);
}

TEST(test_sprite_miss_outside) {
    Sprite s = unit_sprite();
    HitRecord rec;
    ASSERT(!sprite_intersect(&s, NULL, ray_at(0.6f, 1), 0, 100, &rec));  /* right of it */
    ASSERT(!sprite_intersect(&s, NULL, ray_at(0, 2.1f), 0, 100, &rec));  /* above */
    ASSERT(!sprite_intersect(&s, NULL, ray_at(0, -0.1f), 0, 100, &rec)); /* below */
    ASSERT(!sprite_intersect(&s, NULL, ray_at(0, 1), 0, 4.0f, &rec));    /* beyond t_max */
}

TEST(test_sprite_faces_any_direction) {
    Sprite s = unit_sprite();
    HitRecord rec;
    Ray from_side = {vec3(5, 1, 0), vec3(-1, 0, 0)};
    ASSERT(sprite_intersect(&s, NULL, from_side, 0, 100, &rec));
    ASSERT_NEAR(rec.normal.x, 1.0f, 1e-5f);
}

TEST(test_sprite_vertical_ray_misses) {
    Sprite s = unit_sprite();
    HitRecord rec;
    Ray down = {vec3(0, 5, 0), vec3(0, -1, 0)};
    ASSERT(!sprite_intersect(&s, NULL, down, 0, 100, &rec));
}

/* Viewer's left half is opaque, right half is keyed (a hole) */
TEST(test_sprite_color_key_hole) {
    Sprite s = unit_sprite();
    Texture t = half_keyed();
    HitRecord rec;
    ASSERT(sprite_intersect(&s, &t, ray_at(-0.25f, 1), 0, 100, &rec));
    ASSERT(!sprite_intersect(&s, &t, ray_at(0.25f, 1), 0, 100, &rec));
    texture_free(&t);
}

/* Two-frame sheet: frame 1 maps to the right half of the texture */
TEST(test_sprite_frame_selects_column) {
    Sprite s = unit_sprite();
    s.frame_count = 2;
    s.frame = 1;
    HitRecord rec;
    ASSERT(sprite_intersect(&s, NULL, ray_at(0, 1), 0, 100, &rec));
    ASSERT(rec.uv.u >= 0.5f && rec.uv.u < 1.0f);
}

int main(void) {
    TEST_SUITE("sprite");
    RUN_TEST(test_sprite_hit_center);
    RUN_TEST(test_sprite_miss_outside);
    RUN_TEST(test_sprite_faces_any_direction);
    RUN_TEST(test_sprite_vertical_ray_misses);
    RUN_TEST(test_sprite_color_key_hole);
    RUN_TEST(test_sprite_frame_selects_column);
    TEST_REPORT();
}
