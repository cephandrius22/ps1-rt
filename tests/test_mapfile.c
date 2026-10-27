#include "test_framework.h"
#include "mapfile.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST_MAP "tests/test.map"
#define BAD_MAP  "build/tests/bad.map"

static Scene s;
static LevelInfo info;

static void load_fixture(void) {
    scene_init(&s);
    ASSERT_EQ_INT(mapfile_load(&s, TEST_MAP, &info), 0);
}

TEST(test_map_axis_conversion) {
    Vec3 p = map_to_engine(vec3(32, 64, 96)); /* x east, y north, z up */
    ASSERT_NEAR(p.x, 1.0f, 1e-6f);
    ASSERT_NEAR(p.y, 3.0f, 1e-6f);  /* up */
    ASSERT_NEAR(p.z, -2.0f, 1e-6f); /* north = -Z */
    ASSERT_NEAR(map_angle_to_yaw(90), 0.0f, 1e-6f); /* facing north = facing -Z */
}

/* 6 wall brushes x 12 tris, minus the skipped ceiling top, plus a
 * 12-tri pillar from a TrenchBroom layer (func_group) */
TEST(test_map_brush_geometry) {
    load_fixture();
    ASSERT_EQ_INT(s.static_bvh.tri_count, 6 * 12 - 2 + 12);

    /* Standing at spawn, the floor is right below */
    Ray down = {vec3(0, 1, 4), vec3(0, -1, 0)};
    HitRecord rec = scene_intersect(&s, down, 0.001f, 10.0f);
    ASSERT(rec.hit);
    ASSERT_NEAR(rec.point.y, 0.0f, 0.02f); /* within PS1 vertex jitter */
    ASSERT(rec.normal.y > 0.99f);           /* faces up, out of the brush */
    scene_free(&s);
}

TEST(test_map_spawn_and_light) {
    load_fixture();
    ASSERT_NEAR(info.spawn_pos.z, 4.0f, 1e-5f);
    ASSERT_NEAR(info.spawn_yaw, 0.0f, 1e-5f);

    ASSERT_EQ_INT(s.lights.count, 1);
    const PointLight *l = &s.lights.items[0];
    ASSERT_NEAR(l->position.y, 3.0f, 1e-5f);
    ASSERT_NEAR(l->radius, 10.0f, 1e-5f);          /* 320 units */
    ASSERT_NEAR(l->color.x, 1.0f, 1e-5f);          /* 0..255 color normalized */
    ASSERT_NEAR(l->color.y, 128.0f / 255.0f, 1e-3f);
    ASSERT_NEAR(l->anchor.y, 3.5f, 1e-5f);         /* 16-unit cord above */
    scene_free(&s);
}

TEST(test_map_entities) {
    load_fixture();
    ASSERT_EQ_INT(s.entities.count, 3); /* unknown class skipped */

    const Entity *ghoul = &s.entities.items[0];
    ASSERT_EQ_INT(ghoul->type, ENTITY_GHOUL);
    ASSERT_NEAR(ghoul->position.z, -4.0f, 1e-5f);
    ASSERT_NEAR(ghoul->health, 40.0f, 1e-5f);

    const Entity *decor = &s.entities.items[1];
    ASSERT_NEAR(decor->decor.width, 1.0f, 1e-5f);  /* lengths: units → meters */
    ASSERT_NEAR(decor->decor.height, 1.5f, 1e-5f);

    const Entity *health = &s.entities.items[2];
    ASSERT_NEAR(health->position.y, 0.5f, 1e-5f);
    ASSERT_NEAR(s.materials[health->material_id].color.x, 0.9f, 1e-5f); /* type default */
    scene_free(&s);
}

/* Texture repeats every texture-width map units, matching the editor */
TEST(test_map_uv_scale) {
    load_fixture();
    const BVH *b = &s.static_bvh;
    bool checked = false;
    for (int i = 0; i < b->tri_count && !checked; i++) {
        const Triangle *t = &b->tris[i];
        const Material *m = &s.materials[t->material_id];
        if (t->normal.y < 0.99f || fabsf(t->v0.y) > 0.05f || m->texture_id < 0) {
            continue;
        }
        int tex_w = s.textures[m->texture_id].width;
        float dx_units = (t->v1.x - t->v0.x) * MAP_UNITS_PER_METER;
        ASSERT_NEAR(t->uv1.u - t->uv0.u, dx_units / tex_w, 0.02f);
        checked = true;
    }
    ASSERT(checked);
    scene_free(&s);
}

TEST(test_map_malformed) {
    FILE *f = fopen(BAD_MAP, "w");
    fputs("{ \"classname\" \"worldspawn\"\n{ ( 0 0 0 ) oops }\n", f);
    fclose(f);

    Scene bad;
    scene_init(&bad);
    ASSERT_EQ_INT(mapfile_load(&bad, BAD_MAP, &info), -1);
    scene_free(&bad);
    remove(BAD_MAP);

    Scene missing;
    scene_init(&missing);
    ASSERT_EQ_INT(mapfile_load(&missing, "tests/no_such.map", &info), -1);
    scene_free(&missing);
}

int main(void) {
    TEST_SUITE("mapfile");
    RUN_TEST(test_map_axis_conversion);
    RUN_TEST(test_map_brush_geometry);
    RUN_TEST(test_map_spawn_and_light);
    RUN_TEST(test_map_entities);
    RUN_TEST(test_map_uv_scale);
    RUN_TEST(test_map_malformed);
    TEST_REPORT();
}
