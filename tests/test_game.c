#include "test_framework.h"
#include "game.h"
#include <math.h>

#define TEST_LEVEL "tests/test.level"
#define TICK (1.0f / 60.0f)

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

    game_tick(&g, &idle, TICK);
    game_tick(&g, &idle, TICK);

    ASSERT_NEAR(g.time, 2.0f * TICK, 1e-6f);
    game_free(&g);
}

TEST(test_game_tick_collects_events) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    PlayerCmd fire = {.buttons = BTN_ATTACK};

    game_tick(&g, &fire, TICK);
    ASSERT(g.events.count >= 1);
    ASSERT(g.events.items[0].type == EV_WEAPON_FIRED);

    /* Next tick starts with a fresh queue */
    PlayerCmd idle = {0};
    game_tick(&g, &idle, TICK);
    ASSERT(g.events.count == 0);

    game_free(&g);
}

int main(void) {
    TEST_SUITE("game");
    RUN_TEST(test_game_init_spawns_player);
    RUN_TEST(test_game_init_missing_level);
    RUN_TEST(test_game_tick_advances_time);
    RUN_TEST(test_game_tick_collects_events);
    TEST_REPORT();
}
