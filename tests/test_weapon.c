#include "test_framework.h"
#include "weapon.h"
#include "scene.h"
#include <math.h>

static EventQueue ev; /* sink for events tests don't inspect */
static Scene empty;   /* nothing to hit */

TEST(test_weapon_create) {
    Weapon w = weapon_create(WEAPON_PISTOL);
    ASSERT_NEAR(w.cooldown_timer, 0.0f, 1e-6f);
    ASSERT_NEAR(w.kick, 0.0f, 1e-6f);
    ASSERT(!w.fired_this_frame);
    ASSERT(!w.last_hit.hit);
}

TEST(test_weapon_fire) {
    Weapon w = weapon_create(WEAPON_PISTOL);
    Pose aim = {.pos = vec3(0, 1, 0)};
    PlayerCmd cmd = {.buttons = BTN_ATTACK};

    weapon_update(&w, &cmd, aim, &empty, 1.0f / 60.0f, &ev);

    ASSERT(w.fired_this_frame);
    ASSERT(w.cooldown_timer > 0);
    ASSERT(w.kick > 0);
}

TEST(test_weapon_cooldown) {
    Weapon w = weapon_create(WEAPON_PISTOL);
    Pose aim = {.pos = vec3(0, 1, 0)};
    PlayerCmd cmd = {.buttons = BTN_ATTACK};

    /* First shot */
    weapon_update(&w, &cmd, aim, &empty, 1.0f / 60.0f, &ev);
    ASSERT(w.fired_this_frame);

    /* Second frame: still holding shoot, but cooldown prevents firing */
    weapon_update(&w, &cmd, aim, &empty, 1.0f / 60.0f, &ev);
    ASSERT(!w.fired_this_frame);
}

TEST(test_weapon_cooldown_expires) {
    Weapon w = weapon_create(WEAPON_PISTOL);
    Pose aim = {.pos = vec3(0, 1, 0)};
    PlayerCmd shoot = {.buttons = BTN_ATTACK};
    PlayerCmd no_shoot = {0};

    /* Fire */
    weapon_update(&w, &shoot, aim, &empty, 1.0f / 60.0f, &ev);
    ASSERT(w.fired_this_frame);

    /* Wait for cooldown to expire */
    for (int i = 0; i < 30; i++)
        weapon_update(&w, &no_shoot, aim, &empty, 1.0f / 60.0f, &ev);

    /* Should be able to fire again */
    weapon_update(&w, &shoot, aim, &empty, 1.0f / 60.0f, &ev);
    ASSERT(w.fired_this_frame);
}

TEST(test_weapon_kick_recovers) {
    Weapon w = weapon_create(WEAPON_PISTOL);
    Pose aim = {.pos = vec3(0, 1, 0)};
    PlayerCmd shoot = {.buttons = BTN_ATTACK};
    PlayerCmd no_input = {0};

    weapon_update(&w, &shoot, aim, &empty, 1.0f / 60.0f, &ev);
    float kick_after_shot = w.kick;
    ASSERT(kick_after_shot > 0);

    /* Let kick recover */
    for (int i = 0; i < 60; i++)
        weapon_update(&w, &no_input, aim, &empty, 1.0f / 60.0f, &ev);

    ASSERT(w.kick < kick_after_shot);
    ASSERT_NEAR(w.kick, 0.0f, 1e-6f);
}

TEST(test_weapon_no_fire_without_input) {
    Weapon w = weapon_create(WEAPON_PISTOL);
    Pose aim = {.pos = vec3(0, 1, 0)};
    PlayerCmd cmd = {0};

    weapon_update(&w, &cmd, aim, &empty, 1.0f / 60.0f, &ev);

    ASSERT(!w.fired_this_frame);
    ASSERT_NEAR(w.cooldown_timer, 0.0f, 1e-6f);
}

TEST(test_weapon_hitscan_hits_world) {
    /* Build a simple scene: one triangle straight ahead */
    Mesh m;
    mesh_init(&m);
    Vec3 normal = vec3(0, 0, 1);
    Triangle tri = {
        .v0 = vec3(-5, -5, -5), .v1 = vec3(5, -5, -5), .v2 = vec3(0, 5, -5),
        .uv0 = {0, 0}, .uv1 = {1, 0}, .uv2 = {0.5f, 1},
        .normal = normal, .material_id = 0
    };
    mesh_add_tri(&m, tri);

    Scene s;
    scene_init(&s);
    scene_build_static(&s, m.tris, m.count);

    Weapon w = weapon_create(WEAPON_PISTOL);
    Pose aim = {.pos = vec3(0, 0, 0)};
    PlayerCmd cmd = {.buttons = BTN_ATTACK};

    weapon_update(&w, &cmd, aim, &s, 1.0f / 60.0f, &ev);

    ASSERT(w.fired_this_frame);
    ASSERT(w.last_hit.hit);
    ASSERT_NEAR(w.last_hit.point.z, -5.0f, 0.1f);

    scene_free(&s);
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
    Scene s;
    scene_init(&s);
    scene_build_static(&s, m.tris, m.count);

    Weapon w = weapon_create(WEAPON_PISTOL);
    Pose aim = {.pos = vec3(0, 0, 0)};
    PlayerCmd cmd = {.buttons = BTN_ATTACK};
    EventQueue q = {0};

    weapon_update(&w, &cmd, aim, &s, 1.0f / 60.0f, &q);

    /* Fired, then impacted world geometry (no entity) */
    ASSERT(q.count == 2);
    ASSERT(q.items[0].type == EV_WEAPON_FIRED);
    ASSERT(q.items[1].type == EV_WEAPON_IMPACT);
    ASSERT_NEAR(q.items[1].pos.z, -5.0f, 0.1f);
    ASSERT(q.items[1].entity == ENTITY_INDEX_NONE);
    ASSERT_NEAR(q.items[1].amount, weapon_def(WEAPON_PISTOL)->damage, 1e-6f);

    scene_free(&s);
    mesh_free(&m);
}

TEST(test_weapon_miss_no_impact) {
    Weapon w = weapon_create(WEAPON_PISTOL);
    Pose aim = {.pos = vec3(0, 0, 0)};
    PlayerCmd cmd = {.buttons = BTN_ATTACK};
    EventQueue q = {0};

    weapon_update(&w, &cmd, aim, &empty, 1.0f / 60.0f, &q);

    ASSERT(q.count == 1);
    ASSERT(q.items[0].type == EV_WEAPON_FIRED);
}

TEST(test_weapon_def_table) {
    const WeaponDef *pistol = weapon_def(WEAPON_PISTOL);
    ASSERT(pistol->pellets >= 1);
    ASSERT(pistol->cooldown > 0);
    ASSERT(pistol->damage > 0);
    ASSERT(pistol->range > 0);
}

TEST(test_weapon_uses_def_cooldown) {
    Weapon w = weapon_create(WEAPON_PISTOL);
    Pose aim = {.pos = vec3(0, 0, 0)};
    PlayerCmd cmd = {.buttons = BTN_ATTACK};

    weapon_update(&w, &cmd, aim, &empty, 1.0f / 60.0f, &ev);

    ASSERT_NEAR(w.cooldown_timer, weapon_def(WEAPON_PISTOL)->cooldown, 1e-6f);
    ASSERT_NEAR(w.kick, weapon_def(WEAPON_PISTOL)->kick, 1e-6f);
}

/* Spread pattern: pellet 0 straight ahead, others `spread` radians off */
TEST(test_weapon_pellet_spread) {
    WeaponDef shotgun = {.pellets = 5, .spread = 0.1f};
    Pose aim = {.pos = vec3(0, 0, 0)};
    Vec3 ahead = vec3(0, 0, -1);

    Ray center = weapon_pellet_ray(aim, &shotgun, 0);
    ASSERT_NEAR(vec3_dot(center.dir, ahead), 1.0f, 1e-5f);

    for (int i = 1; i < shotgun.pellets; i++) {
        Ray r = weapon_pellet_ray(aim, &shotgun, i);
        float off_angle = acosf(vec3_dot(r.dir, ahead));
        ASSERT(off_angle > 0.05f);
        ASSERT(off_angle < 0.15f);
    }

    /* Outer pellets are distinct */
    Ray a = weapon_pellet_ray(aim, &shotgun, 1);
    Ray b = weapon_pellet_ray(aim, &shotgun, 3);
    ASSERT(vec3_dot(a.dir, b.dir) < 0.9999f);
}

int main(void) {
    scene_init(&empty);

    TEST_SUITE("weapon");
    RUN_TEST(test_weapon_create);
    RUN_TEST(test_weapon_fire);
    RUN_TEST(test_weapon_cooldown);
    RUN_TEST(test_weapon_cooldown_expires);
    RUN_TEST(test_weapon_kick_recovers);
    RUN_TEST(test_weapon_no_fire_without_input);
    RUN_TEST(test_weapon_hitscan_hits_world);
    RUN_TEST(test_weapon_fire_events);
    RUN_TEST(test_weapon_miss_no_impact);
    RUN_TEST(test_weapon_def_table);
    RUN_TEST(test_weapon_uses_def_cooldown);
    RUN_TEST(test_weapon_pellet_spread);
    scene_free(&empty);
    TEST_REPORT();
}
