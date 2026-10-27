#include "test_framework.h"
#include "game.h"
#include <math.h>

#define TEST_LEVEL "tests/test.level"

/* Game is large (holds a Scene); keep it off the stack */
static Game g;

TEST(test_game_init_spawns_player) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    ASSERT(g.mode == GAME_PLAYING);

    /* test.level: spawn 5 0 10 90 → eye above feet, yaw in radians */
    ASSERT_NEAR(g.player.view.pos.x, 5.0f, 1e-5f);
    ASSERT_NEAR(g.player.view.pos.y, PLAYER_HEIGHT + PLAYER_EYE_OFFSET, 1e-5f);
    ASSERT_NEAR(g.player.view.pos.z, 10.0f, 1e-5f);
    ASSERT_NEAR(g.player.view.yaw, (float)M_PI / 2.0f, 1e-5f);
    ASSERT(g.scene.entities.count == 2);

    game_free(&g);
}

TEST(test_game_init_missing_level) {
    ASSERT(game_init(&g, "tests/no_such.level") == GAME_LOAD_FAILED);
}

TEST(test_game_tick_advances_time) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    PlayerCmd idle = {0};

    game_tick(&g, &idle);
    game_tick(&g, &idle);

    ASSERT_NEAR(g.time, 2.0f * GAME_TICK_DT, 1e-6f);
    game_free(&g);
}

TEST(test_game_update_collects_events_per_frame) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    PlayerCmd fire = {.buttons = BTN_ATTACK};

    ASSERT(game_update(&g, &fire, GAME_TICK_DT) == 1);
    ASSERT(g.events.count >= 1);
    ASSERT(g.events.items[0].type == EV_WEAPON_FIRED);

    /* Next frame starts with a fresh queue */
    PlayerCmd idle = {0};
    game_update(&g, &idle, GAME_TICK_DT);
    ASSERT(g.events.count == 0);

    game_free(&g);
}

TEST(test_game_update_tick_count) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    PlayerCmd idle = {0};

    /* Half a tick: nothing yet; the second half completes one tick */
    ASSERT(game_update(&g, &idle, GAME_TICK_DT * 0.5f) == 0);
    ASSERT(game_update(&g, &idle, GAME_TICK_DT * 0.5f) == 1);

    /* Three ticks' worth at once */
    ASSERT(game_update(&g, &idle, GAME_TICK_DT * 3.0f + 1e-4f) == 3);

    /* A long hitch is capped, and the backlog is dropped */
    ASSERT(game_update(&g, &idle, 10.0f) == GAME_MAX_TICKS_PER_FRAME);
    ASSERT(g.tick_accum < GAME_TICK_DT);

    game_free(&g);
}

/* Mouse look from a frame that ran no tick must not be lost, and must
 * apply exactly once even if the next frame runs several ticks. */
TEST(test_game_update_look_applies_once) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    float yaw0 = g.player.view.yaw;

    PlayerCmd look = {.yaw_delta = 0.1f};
    PlayerCmd idle = {0};
    game_update(&g, &look, GAME_TICK_DT * 0.5f);
    game_update(&g, &idle, GAME_TICK_DT * 3.0f);

    ASSERT_NEAR(g.player.view.yaw, yaw0 + 0.1f, 1e-5f);
    game_free(&g);
}

/* Impulses (flashlight toggle) fire once, not once per tick */
TEST(test_game_update_impulse_once) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    ASSERT(!g.player.flashlight_on);

    PlayerCmd toggle = {.buttons = BTN_FLASHLIGHT};
    game_update(&g, &toggle, GAME_TICK_DT * 3.0f);

    ASSERT(g.player.flashlight_on);
    game_free(&g);
}

/* Run n idle-ish ticks with the given command */
static void run_ticks(const PlayerCmd *cmd, int n) {
    for (int i = 0; i < n; i++) {
        game_tick(&g, cmd);
    }
}

TEST(test_game_death_and_respawn) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    PlayerCmd idle = {0};
    PlayerCmd attack = {.buttons = BTN_ATTACK};
    float spawn_x = g.player.view.pos.x;

    player_damage(&g.player, PLAYER_MAX_HEALTH, &g.events);
    game_tick(&g, &idle);
    ASSERT(g.mode == GAME_DEAD);

    /* Too soon: attack doesn't respawn yet, view sinks */
    float eye_y = g.player.view.pos.y;
    run_ticks(&attack, 10);
    ASSERT(g.mode == GAME_DEAD);
    ASSERT(g.player.view.pos.y < eye_y);

    /* After the delay, attack reloads the level with a fresh player */
    run_ticks(&idle, GAME_TICK_HZ * 2);
    game_tick(&g, &attack);
    ASSERT(g.mode == GAME_PLAYING);
    ASSERT(player_alive(&g.player));
    ASSERT_NEAR(g.player.health, PLAYER_MAX_HEALTH, 1e-6f);
    ASSERT_NEAR(g.player.view.pos.x, spawn_x, 1e-5f);

    game_free(&g);
}

/* Firing kicks the view up; it settles back over time */
TEST(test_game_recoil_kicks_view) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    PlayerCmd fire = {.buttons = BTN_ATTACK};
    PlayerCmd idle = {0};
    float pitch = g.player.view.pitch;

    game_tick(&g, &fire);
    ASSERT(game_view(&g).pitch > pitch);
    ASSERT_NEAR(g.player.view.pitch, pitch, 1e-6f); /* look input untouched */

    run_ticks(&idle, GAME_TICK_HZ * 2);
    ASSERT_NEAR(game_view(&g).pitch, pitch, 1e-6f);

    game_free(&g);
}

int main(void) {
    TEST_SUITE("game");
    RUN_TEST(test_game_init_spawns_player);
    RUN_TEST(test_game_init_missing_level);
    RUN_TEST(test_game_tick_advances_time);
    RUN_TEST(test_game_update_collects_events_per_frame);
    RUN_TEST(test_game_update_tick_count);
    RUN_TEST(test_game_update_look_applies_once);
    RUN_TEST(test_game_update_impulse_once);
    RUN_TEST(test_game_death_and_respawn);
    RUN_TEST(test_game_recoil_kicks_view);
    TEST_REPORT();
}
