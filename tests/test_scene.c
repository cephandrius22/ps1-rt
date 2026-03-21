#include "test_framework.h"
#include "../src/scene.h"

static void add_floor_tri(Mesh *m) {
    mesh_init(m);
    Triangle t = {
        .v0 = vec3(-10, 0, -10), .v1 = vec3(10, 0, -10), .v2 = vec3(0, 0, 10),
        .uv0 = {0,0}, .uv1 = {1,0}, .uv2 = {0.5f,1},
        .normal = vec3(0, 1, 0), .material_id = 0
    };
    mesh_add_tri(m, t);
}

TEST(test_scene_init_free) {
    Scene s;
    scene_init(&s);
    ASSERT(s.entities.count == 0);
    ASSERT(s.lights.count == 0);
    scene_free(&s);
}

TEST(test_scene_static_intersect) {
    Scene s;
    scene_init(&s);

    Mesh m;
    add_floor_tri(&m);
    scene_build_static(&s, m.tris, m.count);

    /* Ray pointing down should hit the floor */
    Ray down = {vec3(0, 5, 0), vec3(0, -1, 0)};
    HitRecord rec = scene_intersect(&s, down, 0.001f, 100.0f);
    ASSERT(rec.hit);
    ASSERT_NEAR(rec.point.y, 0, 0.01f);

    /* Ray pointing up should miss */
    Ray up = {vec3(0, 5, 0), vec3(0, 1, 0)};
    rec = scene_intersect(&s, up, 0.001f, 100.0f);
    ASSERT(!rec.hit);

    mesh_free(&m);
    scene_free(&s);
}

TEST(test_scene_two_level_bvh) {
    Scene s;
    scene_init(&s);

    /* Static: floor at y=0 */
    Mesh m;
    add_floor_tri(&m);
    scene_build_static(&s, m.tris, m.count);

    /* Dynamic: entity above floor */
    Entity prop = {
        .type = ENTITY_PROP, .position = vec3(0, 0, 0),
        .rotation_y = 0, .active = true, .material_id = 0,
        .mesh_id = ENTITY_PROP
    };
    entity_add(&s.entities, prop);
    scene_rebuild_dynamic(&s);
    ASSERT(s.has_dynamic);

    /* Ray from above should hit the entity (prop is a box from y=0 to y=0.6) */
    Ray down = {vec3(0, 5, 0), vec3(0, -1, 0)};
    HitRecord rec = scene_intersect(&s, down, 0.001f, 100.0f);
    ASSERT(rec.hit);
    ASSERT(rec.point.y > -0.01f); /* Should hit entity top, not floor */

    mesh_free(&m);
    scene_free(&s);
}

TEST(test_scene_occluded) {
    Scene s;
    scene_init(&s);

    Mesh m;
    add_floor_tri(&m);
    scene_build_static(&s, m.tris, m.count);

    /* Shadow ray toward floor — should be occluded */
    Ray down = {vec3(0, 5, 0), vec3(0, -1, 0)};
    ASSERT(scene_occluded(&s, down, 0.001f, 100.0f));

    /* Shadow ray away from floor — should not be occluded */
    Ray up = {vec3(0, 5, 0), vec3(0, 1, 0)};
    ASSERT(!scene_occluded(&s, up, 0.001f, 100.0f));

    mesh_free(&m);
    scene_free(&s);
}

TEST(test_scene_dynamic_rebuild) {
    Scene s;
    scene_init(&s);

    Mesh m;
    add_floor_tri(&m);
    scene_build_static(&s, m.tris, m.count);

    /* No entities: no dynamic BVH */
    scene_rebuild_dynamic(&s);
    ASSERT(!s.has_dynamic);

    /* Add entity, rebuild */
    Entity prop = {
        .type = ENTITY_PROP, .position = vec3(5, 0, 5),
        .rotation_y = 0, .active = true, .mesh_id = ENTITY_PROP
    };
    entity_add(&s.entities, prop);
    scene_rebuild_dynamic(&s);
    ASSERT(s.has_dynamic);
    ASSERT(s.dynamic_tri_count > 0);

    /* Deactivate entity, rebuild — no more dynamic */
    s.entities.items[0].active = false;
    scene_rebuild_dynamic(&s);
    ASSERT(!s.has_dynamic);

    mesh_free(&m);
    scene_free(&s);
}

int main(void) {
    TEST_SUITE("scene");
    RUN_TEST(test_scene_init_free);
    RUN_TEST(test_scene_static_intersect);
    RUN_TEST(test_scene_two_level_bvh);
    RUN_TEST(test_scene_occluded);
    RUN_TEST(test_scene_dynamic_rebuild);
    TEST_REPORT();
}
