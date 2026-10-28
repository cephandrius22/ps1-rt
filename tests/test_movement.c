#include "test_framework.h"
#include "movement.h"
#include "player.h"

#define TICK (1.0f / 60.0f)

static Scene s;
static Mesh m;

/* Axis-aligned solid box (outer faces), like the .level `box` directive */
static void add_box(Vec3 lo, Vec3 hi) {
    Vec3 v[8] = {
        vec3(lo.x, lo.y, lo.z), vec3(hi.x, lo.y, lo.z), vec3(hi.x, hi.y, lo.z), vec3(lo.x, hi.y, lo.z),
        vec3(lo.x, lo.y, hi.z), vec3(hi.x, lo.y, hi.z), vec3(hi.x, hi.y, hi.z), vec3(lo.x, hi.y, hi.z),
    };
    int faces[6][4] = {{4,5,6,7}, {1,0,3,2}, {0,4,7,3}, {5,1,2,6}, {7,6,2,3}, {0,1,5,4}};
    for (int f = 0; f < 6; f++) {
        Vec3 a = v[faces[f][0]], b = v[faces[f][1]], c = v[faces[f][2]], d = v[faces[f][3]];
        Vec3 n = vec3_normalize(vec3_cross(vec3_sub(b, a), vec3_sub(c, a)));
        mesh_add_tri(&m, (Triangle){.v0 = a, .v1 = b, .v2 = c, .normal = n});
        mesh_add_tri(&m, (Triangle){.v0 = a, .v1 = c, .v2 = d, .normal = n});
    }
}

/* Floor slab, three 0.5 m steps going -Z, and a 1 m ledge going +X */
static void build_world(void) {
    scene_init(&s);
    mesh_init(&m);
    add_box(vec3(-20, -1, -20), vec3(20, 0, 20));
    add_box(vec3(-2, 0, -10), vec3(2, 0.5f, -6));
    add_box(vec3(-2, 0, -10), vec3(2, 1.0f, -7));
    add_box(vec3(-2, 0, -10), vec3(2, 1.5f, -8));
    add_box(vec3(4, 0, -2), vec3(6, 1.0f, 2));
    scene_build_static(&s, m.tris, m.count);
}

static void free_world(void) {
    mesh_free(&m);
    scene_free(&s);
}

static const BodyShape body = {0.3f, 1.6f, 0.6f};

TEST(test_movement_ground) {
    build_world();
    ASSERT_NEAR(movement_ground(&s, vec3(0, 0, 0), 0.6f, 1.0f), 0.0f, 1e-4f);
    ASSERT_NEAR(movement_ground(&s, vec3(0, 0, -6.5f), 0.6f, 1.0f), 0.5f, 1e-4f); /* step up */
    ASSERT(movement_ground(&s, vec3(5, 0, 0), 0.6f, 1.0f) == MOVEMENT_NO_GROUND
           || movement_ground(&s, vec3(5, 0, 0), 0.6f, 1.0f) < 0.6f); /* 1 m ledge out of reach */
    free_world();
}

TEST(test_movement_steps_are_not_walls) {
    build_world();
    Vec3 feet = movement_slide(&s, vec3(0, 0, -5), vec3(0, 0, -1), body);
    ASSERT(feet.z < -5.9f); /* walked onto the 0.5 m step */
    free_world();
}

TEST(test_movement_ledge_blocks) {
    build_world();
    Vec3 feet = movement_slide(&s, vec3(3, 0, 0), vec3(2, 0, 0), body);
    ASSERT_NEAR(feet.x, 4.0f - body.radius, 1e-3f);
    free_world();
}

/* The player walks up all three steps to the 1.5 m platform */
TEST(test_movement_player_climbs_stairs) {
    build_world();
    Player p = player_create(vec3(0, PLAYER_HEIGHT + PLAYER_EYE_OFFSET, -4));
    p.foot_y = 0;
    p.on_ground = true;
    PlayerCmd fwd = {.forward = 1};
    EventQueue ev = {0};
    for (int i = 0; i < 120 && p.view.pos.z > -9.0f; i++) { /* stop on the top step */
        player_update(&p, &fwd, TICK, &s, &ev);
    }
    ASSERT(p.view.pos.z <= -9.0f);
    ASSERT_NEAR(p.foot_y, 1.5f, 1e-3f);
    free_world();
}

/* A long fall still lands on the floor (search starts at the old feet) */
TEST(test_movement_player_lands_from_height) {
    build_world();
    Player p = player_create(vec3(10, 30.0f, 10));
    PlayerCmd idle = {0};
    EventQueue ev = {0};
    for (int i = 0; i < 240; i++) {
        player_update(&p, &idle, TICK, &s, &ev);
    }
    ASSERT_NEAR(p.foot_y, 0.0f, 1e-3f);
    ASSERT(p.on_ground);
    free_world();
}

int main(void) {
    TEST_SUITE("movement");
    RUN_TEST(test_movement_ground);
    RUN_TEST(test_movement_steps_are_not_walls);
    RUN_TEST(test_movement_ledge_blocks);
    RUN_TEST(test_movement_player_climbs_stairs);
    RUN_TEST(test_movement_player_lands_from_height);
    TEST_REPORT();
}
