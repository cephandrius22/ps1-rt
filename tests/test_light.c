#include "test_framework.h"
#include <string.h>
#include "light.h"

TEST(test_light_list_init) {
    LightList list;
    light_list_init(&list);
    ASSERT(list.count == 0);
    light_list_free(&list);
}

TEST(test_light_add) {
    LightList list;
    light_list_init(&list);

    Light pl = {.position = vec3(1, 2, 3), .color = vec3(1, 0, 0),
                     .intensity = 2.0f, .radius = 10.0f};
    int idx = light_add(&list, pl);
    ASSERT(idx == 0);
    ASSERT(list.count == 1);
    ASSERT_NEAR(list.items[0].intensity, 2.0f, 1e-6f);

    light_list_free(&list);
}

TEST(test_light_attenuation_zero_at_radius) {
    Light pl = {.radius = 10.0f};
    float atten = light_attenuation(&pl, 10.0f);
    ASSERT_NEAR(atten, 0, 1e-6f);
}

TEST(test_light_attenuation_max_at_zero) {
    Light pl = {.radius = 10.0f};
    float atten = light_attenuation(&pl, 0);
    ASSERT(atten > 0.5f); /* Should be close to 1 at distance 0 */
}

TEST(test_light_attenuation_decreases) {
    Light pl = {.radius = 10.0f};
    float a1 = light_attenuation(&pl, 1.0f);
    float a2 = light_attenuation(&pl, 5.0f);
    float a3 = light_attenuation(&pl, 9.0f);
    ASSERT(a1 > a2);
    ASSERT(a2 > a3);
    ASSERT(a3 > 0);
}

TEST(test_light_no_flicker) {
    Light pl = {.intensity = 3.0f, .flicker_speed = 0, .flicker_amount = 0};
    float eff = light_effective_intensity(&pl, 1.0f);
    ASSERT_NEAR(eff, 3.0f, 1e-6f);
}

TEST(test_light_flicker_varies) {
    Light pl = {.intensity = 2.0f, .flicker_speed = 5.0f, .flicker_amount = 0.5f};
    float vals[10];
    bool varies = false;
    for (int i = 0; i < 10; i++) {
        vals[i] = light_effective_intensity(&pl, (float)i * 0.1f);
        if (i > 0 && vals[i] != vals[i-1]) {
            varies = true;
        }
        ASSERT(vals[i] > 0);
        ASSERT(vals[i] <= 2.0f + 0.01f);
    }
    ASSERT(varies);
}

TEST(test_light_beyond_radius) {
    Light pl = {.radius = 5.0f};
    float atten = light_attenuation(&pl, 6.0f);
    ASSERT_NEAR(atten, 0, 1e-6f);
}

TEST(test_light_swing_updates_position) {
    LightList list;
    light_list_init(&list);

    Light swing = {
        .position = vec3(0, 2, 0), .color = vec3(1,1,1),
        .intensity = 1.0f, .radius = 10.0f,
        .anchor = vec3(0, 4, 0), .cord_length = 2.0f,
        .swing_speed = 3.0f, .swing_angle = 0.5f
    };
    light_add(&list, swing);

    /* At time 0, x swing is centered but z has phase offset */
    light_update(&list, 0, 0);
    ASSERT_NEAR(list.items[0].position.x, 0, 0.01f);

    /* Record initial position, then check it moves */
    Vec3 pos0 = list.items[0].position;
    light_update(&list, 1.0f, 0);
    Vec3 pos1 = list.items[0].position;
    /* Should have moved from the initial position */
    float moved = vec3_length(vec3_sub(pos1, pos0));
    ASSERT(moved > 0.01f);

    /* Should stay within cord length of anchor */
    Vec3 diff = vec3_sub(list.items[0].position, swing.anchor);
    float dist = vec3_length(diff);
    ASSERT_NEAR(dist, swing.cord_length, 0.1f);

    light_list_free(&list);
}

TEST(test_light_no_swing_unchanged) {
    LightList list;
    light_list_init(&list);

    Light stationary = {
        .position = vec3(5, 3, 7), .color = vec3(1,1,1),
        .intensity = 1.0f, .radius = 10.0f,
        .swing_speed = 0
    };
    light_add(&list, stationary);

    light_update(&list, 5.0f, 0);
    ASSERT_NEAR(list.items[0].position.x, 5.0f, 1e-6f);
    ASSERT_NEAR(list.items[0].position.y, 3.0f, 1e-6f);
    ASSERT_NEAR(list.items[0].position.z, 7.0f, 1e-6f);

    light_list_free(&list);
}

TEST(test_light_temporary_fades_and_expires) {
    LightList list;
    light_list_init(&list);
    light_add(&list, (Light){.intensity = 1.0f, .radius = 5});
    light_add_temporary(&list, (Light){.intensity = 4.0f, .radius = 5}, 0.1f);
    light_add(&list, (Light){.intensity = 2.0f, .radius = 5});
    ASSERT_EQ_INT(list.count, 3);

    light_update(&list, 0, 0.05f);
    ASSERT_EQ_INT(list.count, 3);
    ASSERT_NEAR(light_effective_intensity(&list.items[1], 0), 2.0f, 1e-4f); /* half faded */

    light_update(&list, 0, 0.06f);
    ASSERT_EQ_INT(list.count, 2); /* expired and removed */
    float total = list.items[0].intensity + list.items[1].intensity;
    ASSERT_NEAR(total, 3.0f, 1e-6f); /* both permanent lights kept */

    light_list_free(&list);
}

/* Switchable lights: named, optionally starting off; off gives nothing */
TEST(test_light_keys_name_and_off) {
    Light l = light_from_keys(vec3(0, 2, 0), vec3(1, 1, 1), 2.0f, 8.0f,
                                   "targetname=hall off=1");
    ASSERT(strcmp(l.targetname, "hall") == 0);
    ASSERT(l.off);
    ASSERT_NEAR(light_effective_intensity(&l, 0), 0.0f, 1e-6f);

    l.off = false;
    ASSERT_NEAR(light_effective_intensity(&l, 0), 2.0f, 1e-6f);
}

/* Spots: full on the axis, fading to nothing at the cone's edge;
 * point lights shine everywhere */
TEST(test_light_cone) {
    Light point = {.radius = 5};
    ASSERT_NEAR(light_cone(&point, vec3(1, 0, 0)), 1.0f, 1e-6f);

    Light spot = light_spot(vec3(0, 2, 0), vec3(0, -1, 0), vec3(1, 1, 1), 1, 10,
                            (float)MATH_PI / 4);
    ASSERT_NEAR(light_cone(&spot, vec3(0, -1, 0)), 1.0f, 1e-5f);
    float half = light_cone(&spot, vec3_normalize(vec3(0.2f, -1, 0)));
    ASSERT(half > 0 && half < 1);
    ASSERT_NEAR(light_cone(&spot, vec3(1, 0, 0)), 0.0f, 1e-6f);   /* outside */
}

/* cone= turns a level light into a spot; aim defaults to straight down */
TEST(test_light_keys_spot) {
    Light l = light_from_keys(vec3(0, 3, 0), vec3(1, 1, 1), 1, 8, "cone=30");
    ASSERT_EQ_INT(l.kind, LIGHT_SPOT);
    ASSERT_NEAR(l.aim.y, -1.0f, 1e-6f);

    Light aimed = light_from_keys(vec3(0, 3, 0), vec3(1, 1, 1), 1, 8, "cone=30 aim_x=1 aim_y=0");
    ASSERT_NEAR(aimed.aim.x, 1.0f, 1e-6f);
    ASSERT_EQ_INT(light_from_keys(vec3(0, 3, 0), vec3(1, 1, 1), 1, 8, "").kind, LIGHT_POINT);
}

/* falloff 0 means the default, so zero-initialized lights keep their look */
TEST(test_light_falloff_default) {
    Light a = {.radius = 10};
    Light b = {.radius = 10, .falloff = LIGHT_FALLOFF_DEF};
    Light far = {.radius = 10, .falloff = 0.1f};
    ASSERT_NEAR(light_attenuation(&a, 3), light_attenuation(&b, 3), 1e-6f);
    ASSERT(light_attenuation(&far, 3) > light_attenuation(&a, 3));
}

int main(void) {
    TEST_SUITE("light");
    RUN_TEST(test_light_list_init);
    RUN_TEST(test_light_add);
    RUN_TEST(test_light_attenuation_zero_at_radius);
    RUN_TEST(test_light_attenuation_max_at_zero);
    RUN_TEST(test_light_attenuation_decreases);
    RUN_TEST(test_light_no_flicker);
    RUN_TEST(test_light_flicker_varies);
    RUN_TEST(test_light_beyond_radius);
    RUN_TEST(test_light_swing_updates_position);
    RUN_TEST(test_light_no_swing_unchanged);
    RUN_TEST(test_light_temporary_fades_and_expires);
    RUN_TEST(test_light_keys_name_and_off);
    RUN_TEST(test_light_cone);
    RUN_TEST(test_light_keys_spot);
    RUN_TEST(test_light_falloff_default);
    TEST_REPORT();
}
