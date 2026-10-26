#include "test_framework.h"
#include "weapon.h"

static EventQueue ev; /* sink for events tests don't inspect */

TEST(test_weapon_create) {
    Weapon w = weapon_create();
    ASSERT_NEAR(w.cooldown_timer, 0.0f, 1e-6f);
    ASSERT_NEAR(w.kick, 0.0f, 1e-6f);
    ASSERT(!w.fired_this_frame);
    ASSERT(!w.last_hit.hit);
}

TEST(test_weapon_fire) {
    Weapon w = weapon_create();
    Camera cam = camera_create(vec3(0, 1, 0), 0, 0, 1.2f);
    PlayerCmd cmd = {.buttons = BTN_ATTACK};

    weapon_update(&w, &cmd, &cam, NULL, 1.0f / 60.0f, &ev);

    ASSERT(w.fired_this_frame);
    ASSERT(w.cooldown_timer > 0);
    ASSERT(w.kick > 0);
}

TEST(test_weapon_cooldown) {
    Weapon w = weapon_create();
    Camera cam = camera_create(vec3(0, 1, 0), 0, 0, 1.2f);
    PlayerCmd cmd = {.buttons = BTN_ATTACK};

    /* First shot */
    weapon_update(&w, &cmd, &cam, NULL, 1.0f / 60.0f, &ev);
    ASSERT(w.fired_this_frame);

    /* Second frame: still holding shoot, but cooldown prevents firing */
    weapon_update(&w, &cmd, &cam, NULL, 1.0f / 60.0f, &ev);
    ASSERT(!w.fired_this_frame);
}

TEST(test_weapon_cooldown_expires) {
    Weapon w = weapon_create();
    Camera cam = camera_create(vec3(0, 1, 0), 0, 0, 1.2f);
    PlayerCmd shoot = {.buttons = BTN_ATTACK};
    PlayerCmd no_shoot = {0};

    /* Fire */
    weapon_update(&w, &shoot, &cam, NULL, 1.0f / 60.0f, &ev);
    ASSERT(w.fired_this_frame);

    /* Wait for cooldown to expire */
    for (int i = 0; i < 30; i++)
        weapon_update(&w, &no_shoot, &cam, NULL, 1.0f / 60.0f, &ev);

    /* Should be able to fire again */
    weapon_update(&w, &shoot, &cam, NULL, 1.0f / 60.0f, &ev);
    ASSERT(w.fired_this_frame);
}

TEST(test_weapon_kick_recovers) {
    Weapon w = weapon_create();
    Camera cam = camera_create(vec3(0, 1, 0), 0, 0, 1.2f);
    PlayerCmd shoot = {.buttons = BTN_ATTACK};
    PlayerCmd no_input = {0};

    weapon_update(&w, &shoot, &cam, NULL, 1.0f / 60.0f, &ev);
    float kick_after_shot = w.kick;
    ASSERT(kick_after_shot > 0);

    /* Let kick recover */
    for (int i = 0; i < 60; i++)
        weapon_update(&w, &no_input, &cam, NULL, 1.0f / 60.0f, &ev);

    ASSERT(w.kick < kick_after_shot);
    ASSERT_NEAR(w.kick, 0.0f, 1e-6f);
}

TEST(test_weapon_no_fire_without_input) {
    Weapon w = weapon_create();
    Camera cam = camera_create(vec3(0, 1, 0), 0, 0, 1.2f);
    PlayerCmd cmd = {0};

    weapon_update(&w, &cmd, &cam, NULL, 1.0f / 60.0f, &ev);

    ASSERT(!w.fired_this_frame);
    ASSERT_NEAR(w.cooldown_timer, 0.0f, 1e-6f);
}

TEST(test_weapon_hitscan_with_bvh) {
    /* Build a simple scene: one triangle in front of camera */
    Mesh m;
    mesh_init(&m);
    Vec3 normal = vec3(0, 0, 1);
    Triangle tri = {
        .v0 = vec3(-5, -5, -5), .v1 = vec3(5, -5, -5), .v2 = vec3(0, 5, -5),
        .uv0 = {0, 0}, .uv1 = {1, 0}, .uv2 = {0.5f, 1},
        .normal = normal, .material_id = 0
    };
    mesh_add_tri(&m, tri);

    BVH bvh;
    bvh_build(&bvh, m.tris, m.count);

    Weapon w = weapon_create();
    Camera cam = camera_create(vec3(0, 0, 0), 0, 0, 1.2f);
    PlayerCmd cmd = {.buttons = BTN_ATTACK};

    weapon_update(&w, &cmd, &cam, &bvh, 1.0f / 60.0f, &ev);

    ASSERT(w.fired_this_frame);
    ASSERT(w.last_hit.hit);
    ASSERT_NEAR(w.last_hit.point.z, -5.0f, 0.1f);

    bvh_free(&bvh);
    mesh_free(&m);
}

TEST(test_weapon_fire_events) {
    Mesh m;
    mesh_init(&m);
    Triangle tri = {
        .v0 = vec3(-5, -5, -5), .v1 = vec3(5, -5, -5), .v2 = vec3(0, 5, -5),
        .normal = vec3(0, 0, 1)
    };
    mesh_add_tri(&m, tri);
    BVH bvh;
    bvh_build(&bvh, m.tris, m.count);

    Weapon w = weapon_create();
    Camera cam = camera_create(vec3(0, 0, 0), 0, 0, 1.2f);
    PlayerCmd cmd = {.buttons = BTN_ATTACK};
    EventQueue q = {0};

    weapon_update(&w, &cmd, &cam, &bvh, 1.0f / 60.0f, &q);

    /* Fired, then impacted world geometry (no entity) */
    ASSERT(q.count == 2);
    ASSERT(q.items[0].type == EV_WEAPON_FIRED);
    ASSERT(q.items[1].type == EV_WEAPON_IMPACT);
    ASSERT_NEAR(q.items[1].pos.z, -5.0f, 0.1f);
    ASSERT(q.items[1].entity == ENTITY_INDEX_NONE);

    bvh_free(&bvh);
    mesh_free(&m);
}

TEST(test_weapon_miss_no_impact) {
    Weapon w = weapon_create();
    Camera cam = camera_create(vec3(0, 0, 0), 0, 0, 1.2f);
    PlayerCmd cmd = {.buttons = BTN_ATTACK};
    EventQueue q = {0};

    weapon_update(&w, &cmd, &cam, NULL, 1.0f / 60.0f, &q);

    ASSERT(q.count == 1);
    ASSERT(q.items[0].type == EV_WEAPON_FIRED);
}

int main(void) {
    TEST_SUITE("weapon");
    RUN_TEST(test_weapon_create);
    RUN_TEST(test_weapon_fire);
    RUN_TEST(test_weapon_cooldown);
    RUN_TEST(test_weapon_cooldown_expires);
    RUN_TEST(test_weapon_kick_recovers);
    RUN_TEST(test_weapon_no_fire_without_input);
    RUN_TEST(test_weapon_hitscan_with_bvh);
    RUN_TEST(test_weapon_fire_events);
    RUN_TEST(test_weapon_miss_no_impact);
    TEST_REPORT();
}
