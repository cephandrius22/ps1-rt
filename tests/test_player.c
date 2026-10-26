#include "test_framework.h"
#include "player.h"

#define PLAYER_EYE_Y (PLAYER_HEIGHT + PLAYER_EYE_OFFSET)

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

    player_update(&p, &cmd, 1.0f / 60.0f, NULL);

    ASSERT_NEAR(p.cam.position.x, start_x, 1e-6f);
    ASSERT_NEAR(p.cam.position.z, start_z, 1e-6f);
    ASSERT_NEAR(p.cam.position.y, PLAYER_EYE_Y, 1e-6f);
}

TEST(test_player_forward) {
    Player p = grounded_player();
    PlayerCmd cmd = {.forward = 1};

    player_update(&p, &cmd, 1.0f, NULL);

    // Default yaw=0, forward is -Z
    ASSERT(p.cam.position.z < 0);
    ASSERT_NEAR(p.cam.position.x, 0.0f, 1e-4f);
}

TEST(test_player_backward) {
    Player p = grounded_player();
    PlayerCmd cmd = {.forward = -1};

    player_update(&p, &cmd, 1.0f, NULL);

    ASSERT(p.cam.position.z > 0);
}

TEST(test_player_strafe_right) {
    Player p = grounded_player();
    PlayerCmd cmd = {.strafe = 1};

    player_update(&p, &cmd, 1.0f, NULL);

    ASSERT(p.cam.position.x > 0);
}

TEST(test_player_strafe_left) {
    Player p = grounded_player();
    PlayerCmd cmd = {.strafe = -1};

    player_update(&p, &cmd, 1.0f, NULL);

    ASSERT(p.cam.position.x < 0);
}

TEST(test_player_diagonal_normalized) {
    Player p1 = grounded_player();
    Player p2 = grounded_player();

    PlayerCmd fwd_only = {.forward = 1};
    PlayerCmd fwd_right = {.forward = 1, .strafe = 1};

    player_update(&p1, &fwd_only, 1.0f, NULL);
    player_update(&p2, &fwd_right, 1.0f, NULL);

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

    player_update(&full, &full_cmd, 1.0f, NULL);
    player_update(&half, &half_cmd, 1.0f, NULL);

    /* Partial input moves proportionally slower (not normalized up) */
    ASSERT_NEAR(half.cam.position.z, full.cam.position.z * 0.5f, 1e-4f);
}

TEST(test_player_mouse_look_yaw) {
    Player p = grounded_player();
    PlayerCmd cmd = {.yaw_delta = -0.3f};

    player_update(&p, &cmd, 1.0f / 60.0f, NULL);

    ASSERT(p.cam.yaw != 0.0f);
    ASSERT_NEAR(p.cam.pitch, 0.0f, 1e-6f);
}

TEST(test_player_mouse_look_pitch) {
    Player p = grounded_player();
    PlayerCmd cmd = {.pitch_delta = -0.3f};

    player_update(&p, &cmd, 1.0f / 60.0f, NULL);

    ASSERT_NEAR(p.cam.yaw, 0.0f, 1e-6f);
    ASSERT(p.cam.pitch != 0.0f);
}

TEST(test_player_pitch_clamped) {
    Player p = grounded_player();
    PlayerCmd cmd = {.pitch_delta = 300.0f};

    player_update(&p, &cmd, 1.0f / 60.0f, NULL);

    // Pitch should be clamped to +/-1.4
    ASSERT(p.cam.pitch <= 1.4f);
    ASSERT(p.cam.pitch >= -1.4f);
}

TEST(test_player_gravity_fall) {
    // Player in the air should fall to ground
    Player p = player_create(vec3(0, 5.0f, 0));
    PlayerCmd cmd = {0};

    for (int i = 0; i < 200; i++)
        player_update(&p, &cmd, 1.0f / 60.0f, NULL);

    // Should have settled on the y=0 floor
    ASSERT(p.on_ground);
    ASSERT_NEAR(p.foot_y, 0.0f, 1e-6f);
    ASSERT_NEAR(p.cam.position.y, PLAYER_EYE_Y, 1e-6f);
}

TEST(test_player_jump) {
    Player p = grounded_player();
    PlayerCmd cmd = {.buttons = BTN_JUMP};

    player_update(&p, &cmd, 1.0f / 60.0f, NULL);

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
    player_update(&p1, &cmd, 1.0f, NULL);
    player_update(&p2, &cmd, 0.5f, NULL);
    player_update(&p2, &cmd, 0.5f, NULL);

    ASSERT_NEAR(p1.cam.position.z, p2.cam.position.z, 0.01f);
}

int main(void) {
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
    TEST_REPORT();
}
