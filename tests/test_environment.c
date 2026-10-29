#include "test_framework.h"
#include "level.h"
#include "mapfile.h"
#include <stdio.h>

#define LEVEL_TMP "build/tests/env_tmp.level"
#define MAP_TMP   "build/tests/env_tmp.map"

TEST(test_env_defaults_and_sky) {
    Environment env = environment_default();
    ASSERT(env.sun_intensity > 0);
    ASSERT_NEAR(vec3_length(env.sun_dir), 1.0f, 1e-5f);

    /* Straight up = top color, straight down = horizon color */
    Vec3 up = environment_sky(&env, vec3(0, 1, 0));
    Vec3 down = environment_sky(&env, vec3(0, -1, 0));
    ASSERT_NEAR(up.x, env.sky_top.x, 1e-6f);
    ASSERT_NEAR(down.x, env.sky_horizon.x, 1e-6f);
}

TEST(test_env_fog_ramp) {
    Environment env = environment_default();
    ASSERT(environment_set(&env, "fog", "10 20 0.1 0.2 0.3"));
    ASSERT_NEAR(environment_fog(&env, 5), 0.0f, 1e-6f);
    ASSERT_NEAR(environment_fog(&env, 15), 0.5f, 1e-6f);
    ASSERT_NEAR(environment_fog(&env, 50), 1.0f, 1e-6f);
    ASSERT_NEAR(env.fog_color.z, 0.3f, 1e-6f);
}

/* Bad lines change nothing */
TEST(test_env_rejects_bad_values) {
    Environment env = environment_default();
    ASSERT(!environment_set(&env, "fog", "20 10 0 0 0"));   /* end before start */
    ASSERT(!environment_set(&env, "sun", "0 0 0 1 1 1 1")); /* no direction */
    ASSERT(!environment_set(&env, "sky", "1 2"));
    ASSERT(!environment_set(&env, "rain", "1"));
    ASSERT_NEAR(env.fog_start, environment_default().fog_start, 1e-6f);
}

TEST(test_env_from_level_file) {
    FILE *f = fopen(LEVEL_TMP, "w");
    fputs("ambient 0.2\nsun 0 1 0 1 0 0 0.5\nfog 2 4 0 0.5 0\n", f);
    fclose(f);

    Scene s;
    scene_init(&s);
    LevelInfo info;
    level_load(&s, LEVEL_TMP, &info);
    remove(LEVEL_TMP);
    ASSERT_NEAR(s.env.ambient, 0.2f, 1e-6f);
    ASSERT_NEAR(s.env.sun_dir.y, 1.0f, 1e-6f);
    ASSERT_NEAR(s.env.sun_intensity, 0.5f, 1e-6f);
    ASSERT_NEAR(s.env.fog_end, 4.0f, 1e-6f);
    scene_free(&s);
}

/* worldspawn keys: fog in units, sun in map axes (map +Z = engine +Y) */
TEST(test_env_from_map_worldspawn) {
    FILE *f = fopen(MAP_TMP, "w");
    fputs("{\n\"classname\" \"worldspawn\"\n\"fog\" \"64 320 0 0 0\"\n"
          "\"sun\" \"0 0 1 1 1 1 0\"\n\"ambient\" \"0.1\"\n}\n", f);
    fclose(f);

    Scene s;
    scene_init(&s);
    LevelInfo info;
    mapfile_load(&s, MAP_TMP, &info);
    remove(MAP_TMP);
    ASSERT_NEAR(s.env.fog_start, 2.0f, 1e-5f);
    ASSERT_NEAR(s.env.fog_end, 10.0f, 1e-5f);
    ASSERT_NEAR(s.env.sun_dir.y, 1.0f, 1e-5f);
    ASSERT_NEAR(s.env.sun_intensity, 0.0f, 1e-6f);
    ASSERT_NEAR(s.env.ambient, 0.1f, 1e-6f);
    scene_free(&s);
}

int main(void) {
    TEST_SUITE("environment");
    RUN_TEST(test_env_defaults_and_sky);
    RUN_TEST(test_env_fog_ramp);
    RUN_TEST(test_env_rejects_bad_values);
    RUN_TEST(test_env_from_level_file);
    RUN_TEST(test_env_from_map_worldspawn);
    TEST_REPORT();
}
