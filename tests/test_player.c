#include "test_framework.h"
#include "player.h"
#include "scene.h"

#define PLAYER_EYE_Y (PLAYER_HEIGHT + PLAYER_EYE_OFFSET)

static EventQueue ev;     /* sink for events tests don't inspect */
static Scene floor_world; /* flat floor at y=0, shared by movement tests */
static Mesh floor_mesh;

/* Scene with a floor quad at y=0 spanning [-10, 10] on X/Z */
static void floor_scene(Scene *s, Mesh *m) {
    scene_init(s);
    mesh_init(m);
    Vec3 a = vec3(-10, 0, -10), b = vec3(10, 0, -10);
    Vec3 c = vec3(10, 0, 10),   d = vec3(-10, 0, 10);
    mesh_add_tri(m, (Triangle){.v0 = a, .v1 = d, .v2 = c, .normal = vec3(0, 1, 0)});
    mesh_add_tri(m, (Triangle){.v0 = a, .v1 = c, .v2 = b, .normal = vec3(0, 1, 0)});
    scene_build_static(s, m->tris, m->count);
}

/* Helper: create a player on the ground at foot_y=0 */
static Player grounded_player(void) {
    Player p = player_create(vec3(0, PLAYER_EYE_Y, 0));
    p.on_ground = true;
    p.foot_y = 0;
    return p;
}

TEST(test_player_create) {
    Player p = player_create(vec3(1, 2, 3));
    ASSERT_NEAR(p.cam.position.x, 1.0f, 1e-6f);
    ASSERT_NEAR(p.cam.position.y, 2.0f, 1e-6f);
    ASSERT_NEAR(p.cam.position.z, 3.0f, 1e-6f);
    ASSERT_NEAR(p.cam.yaw, 0.0f, 1e-6f);
    ASSERT_NEAR(p.cam.pitch, 0.0f, 1e-6f);
    ASSERT_NEAR(p.cam.fov, 1.2f, 1e-6f);
}

TEST(test_player_no_input_stays_still) {
    Player p = grounded_player();
    PlayerCmd cmd = {0};
    float start_x = p.cam.position.x;
    float start_z = p.cam.position.z;

    player_update(&p, &cmd, 1.0f / 60.0f, &floor_world, &ev);

    ASSERT_NEAR(p.cam.position.x, start_x, 1e-6f);
    ASSERT_NEAR(p.cam.position.z, start_z, 1e-6f);
    ASSERT_NEAR(p.cam.position.y, PLAYER_EYE_Y, 1e-6f);
}

TEST(test_player_forward) {
    Player p = grounded_player();
    PlayerCmd cmd = {.forward = 1};

    player_update(&p, &cmd, 1.0f, &floor_world, &ev);

    // Default yaw=0, forward is -Z
    ASSERT(p.cam.position.z < 0);
    ASSERT_NEAR(p.cam.position.x, 0.0f, 1e-4f);
}

TEST(test_player_backward) {
    Player p = grounded_player();
    PlayerCmd cmd = {.forward = -1};

    player_update(&p, &cmd, 1.0f, &floor_world, &ev);

    ASSERT(p.cam.position.z > 0);
}

TEST(test_player_strafe_right) {
    Player p = grounded_player();
    PlayerCmd cmd = {.strafe = 1};

    player_update(&p, &cmd, 1.0f, &floor_world, &ev);

    ASSERT(p.cam.position.x > 0);
}

TEST(test_player_strafe_left) {
    Player p = grounded_player();
    PlayerCmd cmd = {.strafe = -1};

    player_update(&p, &cmd, 1.0f, &floor_world, &ev);

    ASSERT(p.cam.position.x < 0);
}

TEST(test_player_diagonal_normalized) {
    Player p1 = grounded_player();
    Player p2 = grounded_player();

    PlayerCmd fwd_only = {.forward = 1};
    PlayerCmd fwd_right = {.forward = 1, .strafe = 1};

    player_update(&p1, &fwd_only, 1.0f, &floor_world, &ev);
    player_update(&p2, &fwd_right, 1.0f, &floor_world, &ev);

    // Diagonal movement should be same speed as forward-only
    Vec3 d1 = vec3_sub(p1.cam.position, vec3(0, PLAYER_EYE_Y, 0));
    Vec3 d2 = vec3_sub(p2.cam.position, vec3(0, PLAYER_EYE_Y, 0));
    d1.y = 0; d2.y = 0;  // Ignore Y
    float speed1 = vec3_length(d1);
    float speed2 = vec3_length(d2);
    ASSERT_NEAR(speed1, speed2, 0.01f);
}

TEST(test_player_analog_partial_speed) {
    Player full = grounded_player();
    Player half = grounded_player();
    PlayerCmd full_cmd = {.forward = 1};
    PlayerCmd half_cmd = {.forward = 0.5f};

    player_update(&full, &full_cmd, 1.0f, &floor_world, &ev);
    player_update(&half, &half_cmd, 1.0f, &floor_world, &ev);

    /* Partial input moves proportionally slower (not normalized up) */
    ASSERT_NEAR(half.cam.position.z, full.cam.position.z * 0.5f, 1e-4f);
}

TEST(test_player_mouse_look_yaw) {
    Player p = grounded_player();
    PlayerCmd cmd = {.yaw_delta = -0.3f};

    player_update(&p, &cmd, 1.0f / 60.0f, &floor_world, &ev);

    ASSERT(p.cam.yaw != 0.0f);
    ASSERT_NEAR(p.cam.pitch, 0.0f, 1e-6f);
}

TEST(test_player_mouse_look_pitch) {
    Player p = grounded_player();
    PlayerCmd cmd = {.pitch_delta = -0.3f};

    player_update(&p, &cmd, 1.0f / 60.0f, &floor_world, &ev);

    ASSERT_NEAR(p.cam.yaw, 0.0f, 1e-6f);
    ASSERT(p.cam.pitch != 0.0f);
}

TEST(test_player_pitch_clamped) {
    Player p = grounded_player();
    PlayerCmd cmd = {.pitch_delta = 300.0f};

    player_update(&p, &cmd, 1.0f / 60.0f, &floor_world, &ev);

    // Pitch should be clamped to +/-1.4
    ASSERT(p.cam.pitch <= 1.4f);
    ASSERT(p.cam.pitch >= -1.4f);
}

TEST(test_player_gravity_fall) {
    // Player in the air should fall to ground
    Player p = player_create(vec3(0, 5.0f, 0));
    PlayerCmd cmd = {0};

    for (int i = 0; i < 200; i++)
        player_update(&p, &cmd, 1.0f / 60.0f, &floor_world, &ev);

    // Should have settled on the y=0 floor
    ASSERT(p.on_ground);
    ASSERT_NEAR(p.foot_y, 0.0f, 1e-6f);
    ASSERT_NEAR(p.cam.position.y, PLAYER_EYE_Y, 1e-6f);
}

TEST(test_player_jump) {
    Player p = grounded_player();
    PlayerCmd cmd = {.buttons = BTN_JUMP};

    player_update(&p, &cmd, 1.0f / 60.0f, &floor_world, &ev);

    // Should have left the ground
    ASSERT(!p.on_ground);
    ASSERT(p.velocity.y > 0);
    ASSERT(p.foot_y > 0);
}

TEST(test_player_delta_time_scaling) {
    Player p1 = grounded_player();
    Player p2 = grounded_player();

    PlayerCmd cmd = {.forward = 1};

    // One big step vs two half steps
    player_update(&p1, &cmd, 1.0f, &floor_world, &ev);
    player_update(&p2, &cmd, 0.5f, &floor_world, &ev);
    player_update(&p2, &cmd, 0.5f, &floor_world, &ev);

    ASSERT_NEAR(p1.cam.position.z, p2.cam.position.z, 0.01f);
}

TEST(test_player_footstep_events) {
    Player p = grounded_player();
    PlayerCmd cmd = {.forward = 1};
    EventQueue q = {0};

    /* Walk 2 seconds at PLAYER_SPEED → floor(10 / PLAYER_STEP_DIST) steps */
    for (int i = 0; i < 120; i++) {
        player_update(&p, &cmd, 1.0f / 60.0f, &floor_world, &q);
    }

    int expected = (int)(PLAYER_SPEED * 2.0f / PLAYER_STEP_DIST + 1e-3f);
    ASSERT(q.count == expected || q.count == expected - 1);
    ASSERT(q.items[0].type == EV_FOOTSTEP);
    ASSERT_NEAR(q.items[0].pos.y, 0.0f, 1e-6f); /* at the feet */
}

TEST(test_player_airborne_no_footsteps) {
    Player p = player_create(vec3(0, 50.0f, 0));
    PlayerCmd cmd = {.forward = 1};
    EventQueue q = {0};

    for (int i = 0; i < 30; i++) {
        player_update(&p, &cmd, 1.0f / 60.0f, &floor_world, &q);
    }

    ASSERT(q.count == 0);
}

/* Pickups sit near the floor (e1m1: y=0.5) while the player's eye is
 * at ~1.7; standing next to one must still collect it. */
TEST(test_player_collects_floor_pickup) {
    Scene s;
    Mesh m;
    floor_scene(&s, &m);
    entity_add(&s.entities, entity_spawn(ENTITY_PICKUP_HEALTH, vec3(0, 0.5f, -0.5f), "amount=25"));

    Player p = grounded_player();
    PlayerCmd cmd = {0};
    EventQueue q = {0};
    player_update(&p, &cmd, 1.0f / 60.0f, &s, &q);

    ASSERT(q.count == 1);
    ASSERT(q.items[0].type == EV_PICKUP);
    ASSERT(q.items[0].entity == 0);
    ASSERT_NEAR(q.items[0].amount, 25.0f, 1e-6f);
    ASSERT(!s.entities.items[0].active);

    mesh_free(&m);
    scene_free(&s);
}

/* A pickup well above the player's head is out of reach */
TEST(test_player_ignores_pickup_overhead) {
    Scene s;
    Mesh m;
    floor_scene(&s, &m);
    entity_add(&s.entities, entity_spawn(ENTITY_PICKUP_HEALTH, vec3(0, 3.0f, 0), ""));

    Player p = grounded_player();
    PlayerCmd cmd = {0};
    EventQueue q = {0};
    player_update(&p, &cmd, 1.0f / 60.0f, &s, &q);

    ASSERT(q.count == 0);
    ASSERT(s.entities.items[0].active);

    mesh_free(&m);
    scene_free(&s);
}

int main(void) {
    floor_scene(&floor_world, &floor_mesh);

    TEST_SUITE("player");
    RUN_TEST(test_player_create);
    RUN_TEST(test_player_no_input_stays_still);
    RUN_TEST(test_player_forward);
    RUN_TEST(test_player_backward);
    RUN_TEST(test_player_strafe_right);
    RUN_TEST(test_player_strafe_left);
    RUN_TEST(test_player_diagonal_normalized);
    RUN_TEST(test_player_analog_partial_speed);
    RUN_TEST(test_player_mouse_look_yaw);
    RUN_TEST(test_player_mouse_look_pitch);
    RUN_TEST(test_player_pitch_clamped);
    RUN_TEST(test_player_gravity_fall);
    RUN_TEST(test_player_jump);
    RUN_TEST(test_player_delta_time_scaling);
    RUN_TEST(test_player_footstep_events);
    RUN_TEST(test_player_airborne_no_footsteps);
    RUN_TEST(test_player_collects_floor_pickup);
    RUN_TEST(test_player_ignores_pickup_overhead);
    mesh_free(&floor_mesh);
    scene_free(&floor_world);
    TEST_REPORT();
}
