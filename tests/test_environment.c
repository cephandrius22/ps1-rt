#include "test_framework.h"
#include "level.h"
#include "mapfile.h"
#include <stdio.h>
#include <string.h>

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

/* Ambient per surface from its zone; fog from the eye's zone; a later
 * zone inside an earlier one wins */
TEST(test_env_zones) {
    Environment base = environment_default();
    ZoneList zones;
    zone_list_init(&zones);
    EnvZone room, closet;
    ASSERT(zone_from_keys(&room, vec3(0, 0, 0), vec3(10, 3, 10), &base,
                          "zone ambient=0.2 fog=\"1 5 0 0.3 0\""));
    ASSERT(zone_from_keys(&closet, vec3(2, 0, 2), vec3(3, 3, 3), &base, "ambient=0.01"));
    EnvZone empty;
    ASSERT(!zone_from_keys(&empty, vec3(0, 0, 0), vec3(1, 1, 1), &base, "nothing=1"));
    zone_add(&zones, room);
    zone_add(&zones, closet);

    ASSERT_NEAR(environment_ambient_at(&base, &zones, vec3(5, 1, 5)), 0.2f, 1e-6f);
    ASSERT_NEAR(environment_ambient_at(&base, &zones, vec3(2.5f, 1, 2.5f)), 0.01f, 1e-6f);
    ASSERT_NEAR(environment_ambient_at(&base, &zones, vec3(-5, 1, 0)), base.ambient, 1e-6f);

    /* The closet sets no fog, so the room's fog still applies inside it */
    Environment view = environment_for_view(&base, &zones, vec3(2.5f, 1, 2.5f));
    ASSERT_NEAR(view.fog_end, 5.0f, 1e-6f);
    ASSERT_NEAR(environment_for_view(&base, &zones, vec3(-5, 1, 0)).fog_end, base.fog_end, 1e-6f);
    zone_list_free(&zones);
}

/* Sound: level defaults, keys, and the listener's zone overriding them */
TEST(test_env_sound) {
    Environment base = environment_default();
    ASSERT(environment_set(&base, "reverb", "0.3"));
    ASSERT(environment_set(&base, "ambience", "hum"));
    ASSERT(!environment_set(&base, "reverb", "2"));   /* out of range */
    ASSERT(strcmp(base.ambience, "hum") == 0);

    ZoneList zones;
    zone_list_init(&zones);
    EnvZone cave;
    ASSERT(zone_from_keys(&cave, vec3(0, 0, 0), vec3(5, 3, 5), &base, "reverb=0.8 ambience=drip"));
    zone_add(&zones, cave);

    Environment inside = environment_for_view(&base, &zones, vec3(1, 1, 1));
    ASSERT_NEAR(inside.reverb, 0.8f, 1e-6f);
    ASSERT(strcmp(inside.ambience, "drip") == 0);
    Environment outside = environment_for_view(&base, &zones, vec3(9, 1, 9));
    ASSERT(strcmp(outside.ambience, "hum") == 0);
    zone_list_free(&zones);
}

TEST(test_env_blend) {
    Environment a = environment_default();
    Environment b = a;
    environment_set(&b, "fog", "2 4 1 1 1");
    Environment mid = environment_blend(&a, &b, 0.5f);
    ASSERT_NEAR(mid.fog_start, (a.fog_start + 2) * 0.5f, 1e-5f);
    ASSERT_NEAR(mid.fog_color.x, (a.fog_color.x + 1) * 0.5f, 1e-5f);
}

/* Zones from both formats; .map fog distances are in units */
TEST(test_env_zone_loading) {
    FILE *f = fopen(LEVEL_TMP, "w");
    fputs("zone 0 0 0 4 3 4 ambient=0.3 fog=\"1 2 0 0 0\"\n", f);
    fclose(f);
    Scene s;
    scene_init(&s);
    LevelInfo info;
    level_load(&s, LEVEL_TMP, &info);
    remove(LEVEL_TMP);
    ASSERT_EQ_INT(s.zones.count, 1);
    ASSERT_NEAR(s.zones.items[0].env.fog_end, 2.0f, 1e-6f);
    scene_free(&s);

    f = fopen(MAP_TMP, "w");
    fputs("{\n\"classname\" \"worldspawn\"\n}\n"
          "{\n\"classname\" \"env_zone\"\n\"fog\" \"32 64 0 0 0\"\n{\n"
          "( 64 1 0 ) ( 64 0 0 ) ( 64 0 1 ) trigger 0 0 0 1 1\n"
          "( 0 0 1 ) ( 0 0 0 ) ( 0 1 0 ) trigger 0 0 0 1 1\n"
          "( 0 64 1 ) ( 0 64 0 ) ( 1 64 0 ) trigger 0 0 0 1 1\n"
          "( 1 0 0 ) ( 0 0 0 ) ( 0 0 1 ) trigger 0 0 0 1 1\n"
          "( 1 0 96 ) ( 0 0 96 ) ( 0 1 96 ) trigger 0 0 0 1 1\n"
          "( 0 1 0 ) ( 0 0 0 ) ( 1 0 0 ) trigger 0 0 0 1 1\n}\n}\n", f);
    fclose(f);
    scene_init(&s);
    mapfile_load(&s, MAP_TMP, &info);
    remove(MAP_TMP);
    ASSERT_EQ_INT(s.zones.count, 1);
    ASSERT_NEAR(s.zones.items[0].env.fog_start, 1.0f, 1e-5f);
    ASSERT_NEAR(s.zones.items[0].hi.y, 3.0f, 1e-5f);   /* 96 units tall */
    scene_free(&s);
}

int main(void) {
    TEST_SUITE("environment");
    RUN_TEST(test_env_defaults_and_sky);
    RUN_TEST(test_env_fog_ramp);
    RUN_TEST(test_env_rejects_bad_values);
    RUN_TEST(test_env_from_level_file);
    RUN_TEST(test_env_from_map_worldspawn);
    RUN_TEST(test_env_zones);
    RUN_TEST(test_env_sound);
    RUN_TEST(test_env_blend);
    RUN_TEST(test_env_zone_loading);
    TEST_REPORT();
}
