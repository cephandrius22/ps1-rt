#include "test_framework.h"
#include "../src/player.h"

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
    Player p = player_create(vec3(0, PLAYER_HEIGHT, 0));
    InputState input = {0};
    float start_x = p.cam.position.x;
    float start_z = p.cam.position.z;

    player_update(&p, &input, 1.0f / 60.0f);

    ASSERT_NEAR(p.cam.position.x, start_x, 1e-6f);
    ASSERT_NEAR(p.cam.position.z, start_z, 1e-6f);
    ASSERT_NEAR(p.cam.position.y, PLAYER_HEIGHT, 1e-6f);
}

TEST(test_player_forward) {
    Player p = player_create(vec3(0, PLAYER_HEIGHT, 0));
    InputState input = {.forward = true};

    player_update(&p, &input, 1.0f);

    // Default yaw=0, forward is -Z
    ASSERT(p.cam.position.z < 0);
    ASSERT_NEAR(p.cam.position.x, 0.0f, 1e-4f);
}

TEST(test_player_backward) {
    Player p = player_create(vec3(0, PLAYER_HEIGHT, 0));
    InputState input = {.back = true};

    player_update(&p, &input, 1.0f);

    ASSERT(p.cam.position.z > 0);
}

TEST(test_player_strafe_right) {
    Player p = player_create(vec3(0, PLAYER_HEIGHT, 0));
    InputState input = {.right = true};

    player_update(&p, &input, 1.0f);

    ASSERT(p.cam.position.x > 0);
}

TEST(test_player_strafe_left) {
    Player p = player_create(vec3(0, PLAYER_HEIGHT, 0));
    InputState input = {.left = true};

    player_update(&p, &input, 1.0f);

    ASSERT(p.cam.position.x < 0);
}

TEST(test_player_diagonal_normalized) {
    Player p1 = player_create(vec3(0, PLAYER_HEIGHT, 0));
    Player p2 = player_create(vec3(0, PLAYER_HEIGHT, 0));

    InputState fwd_only = {.forward = true};
    InputState fwd_right = {.forward = true, .right = true};

    player_update(&p1, &fwd_only, 1.0f);
    player_update(&p2, &fwd_right, 1.0f);

    // Diagonal movement should be same speed as forward-only
    Vec3 d1 = vec3_sub(p1.cam.position, vec3(0, PLAYER_HEIGHT, 0));
    Vec3 d2 = vec3_sub(p2.cam.position, vec3(0, PLAYER_HEIGHT, 0));
    d1.y = 0; d2.y = 0;  // Ignore Y
    float speed1 = vec3_length(d1);
    float speed2 = vec3_length(d2);
    ASSERT_NEAR(speed1, speed2, 0.01f);
}

TEST(test_player_mouse_look_yaw) {
    Player p = player_create(vec3(0, PLAYER_HEIGHT, 0));
    InputState input = {.mouse_dx = 100, .mouse_dy = 0};

    player_update(&p, &input, 1.0f / 60.0f);

    ASSERT(p.cam.yaw != 0.0f);
    ASSERT_NEAR(p.cam.pitch, 0.0f, 1e-6f);
}

TEST(test_player_mouse_look_pitch) {
    Player p = player_create(vec3(0, PLAYER_HEIGHT, 0));
    InputState input = {.mouse_dx = 0, .mouse_dy = 100};

    player_update(&p, &input, 1.0f / 60.0f);

    ASSERT_NEAR(p.cam.yaw, 0.0f, 1e-6f);
    ASSERT(p.cam.pitch != 0.0f);
}

TEST(test_player_pitch_clamped) {
    Player p = player_create(vec3(0, PLAYER_HEIGHT, 0));
    InputState input = {.mouse_dx = 0, .mouse_dy = -100000};

    player_update(&p, &input, 1.0f / 60.0f);

    // Pitch should be clamped to ±1.4
    ASSERT(p.cam.pitch <= 1.4f);
    ASSERT(p.cam.pitch >= -1.4f);
}

TEST(test_player_height_maintained) {
    Player p = player_create(vec3(0, 5.0f, 0));
    InputState input = {.forward = true, .right = true};

    for (int i = 0; i < 100; i++)
        player_update(&p, &input, 1.0f / 60.0f);

    // Height should always be PLAYER_HEIGHT regardless of movement
    ASSERT_NEAR(p.cam.position.y, PLAYER_HEIGHT, 1e-6f);
}

TEST(test_player_delta_time_scaling) {
    Player p1 = player_create(vec3(0, PLAYER_HEIGHT, 0));
    Player p2 = player_create(vec3(0, PLAYER_HEIGHT, 0));

    InputState input = {.forward = true};

    // One big step vs two half steps
    player_update(&p1, &input, 1.0f);
    player_update(&p2, &input, 0.5f);
    player_update(&p2, &input, 0.5f);

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
    RUN_TEST(test_player_mouse_look_yaw);
    RUN_TEST(test_player_mouse_look_pitch);
    RUN_TEST(test_player_pitch_clamped);
    RUN_TEST(test_player_height_maintained);
    RUN_TEST(test_player_delta_time_scaling);
    TEST_REPORT();
}
