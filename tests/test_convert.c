#include "test_framework.h"
#include "level_convert.h"
#include "level.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define LEVEL_TMP "build/tests/convert_tmp.level"
#define MAP_TMP   "build/tests/convert_tmp.map"

/* Every directive the converter handles, once */
static const char *source =
    "spawn 0 0 2 90\n"
    "ambient 0.07\nfog 4 12 0.1 0.2 0.3\nsun 0 1 0 1 1 1 0\nreverb 0.4\nambience hum\n"
    "material stone concrete 0.4 0.4 0.4\nmaterial plate metal_panel 0.5 0.5 0.5\n"
    "room -5 0 -5 5 3 5 stone stone stone\n"
    "box -1 0 -1 1 1 1 plate\n"
    "brush func_door 2 0 -5 4 2.5 -4.8 plate opens=side rot=-1.5707963 key=red\n"
    "brush trigger_once -1 0 2 1 2 3 message=\"Mind the gap\" target=lamp\n"
    "zone -5 0 -5 0 3 0 ambient=0.01 fog=\"1 3 0 0 0\"\n"
    "entity ghoul 3 0 3 health=40 rot=1.5707963\n"
    "entity decor -3 0 3 w=0.5\n"
    "entity keycard 0 0 4 color=red\n"
    "entity exit 0 0 -4\n"
    "light 0 2.5 0 1 0.5 0.25 2 8 cone=30 aim_x=1 aim_y=-1 targetname=lamp cord=0.5\n";

static Scene from_level, from_map;

static void load(Scene *s, const char *path) {
    scene_init(s);
    LevelInfo info;
    ASSERT_EQ_INT(level_load(s, path, &info), 0);
}

TEST(test_convert_round_trip) {
    FILE *f = fopen(LEVEL_TMP, "w");
    fputs(source, f);
    fclose(f);
    ASSERT_EQ_INT(level_convert(LEVEL_TMP, MAP_TMP), 0);
    load(&from_level, LEVEL_TMP);
    load(&from_map, MAP_TMP);

    /* Same entities, same places, same keys */
    ASSERT_EQ_INT(from_map.entities.count, from_level.entities.count);
    for (int i = 0; i < from_level.entities.count; i++) {
        const Entity *a = &from_level.entities.items[i], *b = &from_map.entities.items[i];
        ASSERT_EQ_INT(b->type, a->type);
        ASSERT(vec3_length(vec3_sub(a->position, b->position)) < 1e-3f);
        ASSERT(vec3_length(vec3_sub(a->half_size, b->half_size)) < 1e-3f);
        ASSERT(strcmp(a->message, b->message) == 0);
        ASSERT(strcmp(a->target, b->target) == 0);
    }
    const Entity *door = &from_map.entities.items[0];
    ASSERT_NEAR(door->mover.dir.x, 1.0f, 1e-3f);      /* rot → angle → rot */
    ASSERT(strcmp(door->mover.key, "red") == 0);
    ASSERT_NEAR(from_map.entities.items[2].health, 40, 1e-4f);
    ASSERT_NEAR(from_map.entities.items[3].decor.width, 0.5f, 1e-4f); /* length keys */

    /* Light: radius, cone, aim, cord */
    const Light *la = &from_level.lights.items[0], *lb = &from_map.lights.items[0];
    ASSERT_NEAR(lb->radius, la->radius, 1e-4f);
    ASSERT_EQ_INT(lb->kind, LIGHT_SPOT);
    ASSERT(vec3_length(vec3_sub(la->aim, lb->aim)) < 1e-4f);
    ASSERT_NEAR(lb->cord_length, la->cord_length, 1e-4f);
    ASSERT(strcmp(lb->targetname, "lamp") == 0);

    /* Environment and zones */
    ASSERT_NEAR(from_map.env.fog_end, 12.0f, 1e-4f);
    ASSERT_NEAR(from_map.env.sun_dir.y, 1.0f, 1e-4f);
    ASSERT_NEAR(from_map.env.reverb, 0.4f, 1e-6f);
    ASSERT(strcmp(from_map.env.ambience, "hum") == 0);
    ASSERT_EQ_INT(from_map.zones.count, 1);
    ASSERT_NEAR(from_map.zones.items[0].env.fog_end, 3.0f, 1e-4f);
    ASSERT_NEAR(from_map.zones.items[0].hi.y, 3.0f, 1e-4f);

    /* Geometry: the room is still closed around the same floor */
    Ray down = {vec3(3, 1, 0), vec3(0, -1, 0)};
    ASSERT_NEAR(bvh_intersect(&from_map.static_bvh, down, 0, 5).t, 1.0f, 1e-3f);
    Ray side = {vec3(0, 1.5f, 0), vec3(1, 0, 0)};
    ASSERT_NEAR(bvh_intersect(&from_map.static_bvh, side, 0, 20).t, 5.0f, 1e-3f);

    scene_free(&from_level);
    scene_free(&from_map);
    remove(LEVEL_TMP);
    remove(MAP_TMP);
}

int main(void) {
    TEST_SUITE("convert");
    RUN_TEST(test_convert_round_trip);
    TEST_REPORT();
}
