#include "test_framework.h"
#include "settings.h"
#include <stdio.h>

#define SETTINGS_TMP "build/tests/settings_tmp.cfg"

/* Saved values come back; missing ones keep defaults; out of range clamps */
TEST(test_settings_round_trip) {
    Settings s = settings_default();
    s.sensitivity = 1.7f;
    s.volume = 0.25f;
    s.fov = 90;
    ASSERT(settings_save(&s, SETTINGS_TMP));

    Settings back;
    ASSERT(settings_load(&back, SETTINGS_TMP));
    ASSERT_NEAR(back.sensitivity, 1.7f, 1e-5f);
    ASSERT_NEAR(back.volume, 0.25f, 1e-5f);
    ASSERT_NEAR(back.fov, 90, 1e-5f);
    remove(SETTINGS_TMP);
}

TEST(test_settings_partial_and_clamped) {
    FILE *f = fopen(SETTINGS_TMP, "w");
    fputs("# hand edited\nfov 500\nnonsense 3\nvolume -1\n", f);
    fclose(f);

    Settings s;
    ASSERT(settings_load(&s, SETTINGS_TMP));
    ASSERT_NEAR(s.fov, FOV_MAX, 1e-5f);
    ASSERT_NEAR(s.volume, VOLUME_MIN, 1e-5f);
    ASSERT_NEAR(s.sensitivity, settings_default().sensitivity, 1e-5f);
    remove(SETTINGS_TMP);

    ASSERT(!settings_load(&s, "build/tests/no_such.cfg"));
    ASSERT_NEAR(s.fov, settings_default().fov, 1e-5f);
}

int main(void) {
    TEST_SUITE("settings");
    RUN_TEST(test_settings_round_trip);
    RUN_TEST(test_settings_partial_and_clamped);
    TEST_REPORT();
}
