#include "test_framework.h"
#include "../src/light.h"

TEST(test_light_list_init) {
    LightList list;
    light_list_init(&list);
    ASSERT(list.count == 0);
    light_list_free(&list);
}

TEST(test_light_add) {
    LightList list;
    light_list_init(&list);

    PointLight pl = {.position = vec3(1, 2, 3), .color = vec3(1, 0, 0),
                     .intensity = 2.0f, .radius = 10.0f};
    int idx = light_add(&list, pl);
    ASSERT(idx == 0);
    ASSERT(list.count == 1);
    ASSERT_NEAR(list.items[0].intensity, 2.0f, 1e-6f);

    light_list_free(&list);
}

TEST(test_light_attenuation_zero_at_radius) {
    PointLight pl = {.radius = 10.0f};
    float atten = light_attenuation(&pl, 10.0f);
    ASSERT_NEAR(atten, 0, 1e-6f);
}

TEST(test_light_attenuation_max_at_zero) {
    PointLight pl = {.radius = 10.0f};
    float atten = light_attenuation(&pl, 0);
    ASSERT(atten > 0.5f); /* Should be close to 1 at distance 0 */
}

TEST(test_light_attenuation_decreases) {
    PointLight pl = {.radius = 10.0f};
    float a1 = light_attenuation(&pl, 1.0f);
    float a2 = light_attenuation(&pl, 5.0f);
    float a3 = light_attenuation(&pl, 9.0f);
    ASSERT(a1 > a2);
    ASSERT(a2 > a3);
    ASSERT(a3 > 0);
}

TEST(test_light_no_flicker) {
    PointLight pl = {.intensity = 3.0f, .flicker_speed = 0, .flicker_amount = 0};
    float eff = light_effective_intensity(&pl, 1.0f);
    ASSERT_NEAR(eff, 3.0f, 1e-6f);
}

TEST(test_light_flicker_varies) {
    PointLight pl = {.intensity = 2.0f, .flicker_speed = 5.0f, .flicker_amount = 0.5f};
    float vals[10];
    bool varies = false;
    for (int i = 0; i < 10; i++) {
        vals[i] = light_effective_intensity(&pl, (float)i * 0.1f);
        if (i > 0 && vals[i] != vals[i-1]) varies = true;
        ASSERT(vals[i] > 0);
        ASSERT(vals[i] <= 2.0f + 0.01f);
    }
    ASSERT(varies);
}

TEST(test_light_beyond_radius) {
    PointLight pl = {.radius = 5.0f};
    float atten = light_attenuation(&pl, 6.0f);
    ASSERT_NEAR(atten, 0, 1e-6f);
}

TEST(test_light_swing_updates_position) {
    LightList list;
    light_list_init(&list);

    PointLight swing = {
        .position = vec3(0, 2, 0), .color = vec3(1,1,1),
        .intensity = 1.0f, .radius = 10.0f,
        .anchor = vec3(0, 4, 0), .cord_length = 2.0f,
        .swing_speed = 3.0f, .swing_angle = 0.5f
    };
    light_add(&list, swing);

    /* At time 0, x swing is centered but z has phase offset */
    light_update(&list, 0);
    ASSERT_NEAR(list.items[0].position.x, 0, 0.01f);

    /* Record initial position, then check it moves */
    Vec3 pos0 = list.items[0].position;
    light_update(&list, 1.0f);
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

    PointLight stationary = {
        .position = vec3(5, 3, 7), .color = vec3(1,1,1),
        .intensity = 1.0f, .radius = 10.0f,
        .swing_speed = 0
    };
    light_add(&list, stationary);

    light_update(&list, 5.0f);
    ASSERT_NEAR(list.items[0].position.x, 5.0f, 1e-6f);
    ASSERT_NEAR(list.items[0].position.y, 3.0f, 1e-6f);
    ASSERT_NEAR(list.items[0].position.z, 7.0f, 1e-6f);

    light_list_free(&list);
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
    TEST_REPORT();
}
