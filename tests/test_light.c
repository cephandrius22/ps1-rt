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
    TEST_REPORT();
}
