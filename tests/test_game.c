#include "test_framework.h"
#include "game.h"
#include <math.h>
#include <stdio.h>

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
    ASSERT(g.scene.entities.count == 3);

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

/* Shots at a ghoul hurt it, then kill it (pistol: 25 per hit) */
TEST(test_game_shooting_kills_ghoul) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    Pose view = g.player.view;

    /* Ghoul 3 units ahead, centered on the line of sight */
    Vec3 ahead = pose_to_world_dir(view, vec3(0, 0, -1));
    Vec3 base = vec3_add(view.pos, vec3_mul(ahead, 3.0f));
    base.y = view.pos.y - 1.0f;
    int idx = entity_add(&g.scene.entities, entity_spawn(ENTITY_GHOUL, base, "health=40"));
    g.player.on_ground = true; /* hold still: no floor under the spawn */

    PlayerCmd fire = {.buttons = BTN_ATTACK};
    PlayerCmd idle = {0};
    game_update(&g, &fire, GAME_TICK_DT);
    ASSERT(g.scene.entities.items[idx].enemy.state == ENEMY_PAIN);

    /* Wait out the cooldown (briefly: the spawn has no floor), fire again */
    int cooldown_ticks = (int)(weapon_def(WEAPON_PISTOL)->cooldown * GAME_TICK_HZ) + 1;
    for (int i = 0; i < cooldown_ticks; i++) {
        game_update(&g, &idle, GAME_TICK_DT);
    }
    game_update(&g, &fire, GAME_TICK_DT);
    ASSERT(g.scene.entities.items[idx].enemy.state == ENEMY_DEAD);

    bool died = false;
    for (int i = 0; i < g.events.count; i++) {
        died |= g.events.items[i].type == EV_ENEMY_DIED && g.events.items[i].entity == idx;
    }
    ASSERT(died);
    ASSERT_EQ_INT(g.kills, 1);
    game_free(&g);
}

/* Stepping on the exit pad ends the level; input after the delay replays */
TEST(test_game_exit_ends_level) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    ASSERT_EQ_INT(g.kills_total, 0);
    Vec3 feet = vec3(g.player.view.pos.x, g.player.foot_y, g.player.view.pos.z);
    entity_add(&g.scene.entities, entity_spawn(ENTITY_EXIT, feet, ""));

    PlayerCmd idle = {0};
    PlayerCmd use = {.buttons = BTN_USE};
    game_tick(&g, &idle);
    ASSERT(g.mode == GAME_LEVEL_END);

    run_ticks(&use, 10); /* too soon */
    ASSERT(g.mode == GAME_LEVEL_END);

    run_ticks(&idle, GAME_TICK_HZ * 2);
    game_tick(&g, &use);
    ASSERT(g.mode == GAME_PLAYING);
    game_free(&g);
}

/* Hot reload keeps where the player stands; a bad path keeps the old level */
TEST(test_game_reload_keeps_pose) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    g.player.view.pos = vec3(1, 2, 3);
    g.player.view.yaw = 0.7f;
    g.player.health = 10;
    g.scene.entities.items[0].active = false;

    ASSERT(game_reload(&g) == GAME_LOAD_OK);
    ASSERT_NEAR(g.player.view.pos.x, 1, 1e-6f);
    ASSERT_NEAR(g.player.view.pos.z, 3, 1e-6f);
    ASSERT_NEAR(g.player.view.yaw, 0.7f, 1e-6f);
    ASSERT_NEAR(g.player.health, PLAYER_MAX_HEALTH, 1e-6f);
    ASSERT(g.scene.entities.items[0].active); /* world reset */

    snprintf(g.level_path, sizeof(g.level_path), "tests/no_such.level");
    ASSERT(game_reload(&g) == GAME_LOAD_FAILED);
    ASSERT_EQ_INT(g.scene.entities.count, 3); /* still the old level */
    game_free(&g);
}

/* Number keys select only owned weapons; the wheel skips unowned ones */
TEST(test_game_weapon_select) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    PlayerCmd two = {.weapon_slot = 2};

    game_update(&g, &two, GAME_TICK_DT);
    ASSERT_EQ_INT(weapon_target(&g.weapon), WEAPON_PISTOL); /* shotgun not owned */

    inventory_give_weapon(&g.player.inv, WEAPON_SHOTGUN);
    game_update(&g, &two, GAME_TICK_DT);
    ASSERT_EQ_INT(weapon_target(&g.weapon), WEAPON_SHOTGUN);

    PlayerCmd wheel = {.weapon_cycle = 1};
    game_update(&g, &wheel, GAME_TICK_DT);
    ASSERT_EQ_INT(weapon_target(&g.weapon), WEAPON_PISTOL); /* wraps past the end */
    game_free(&g);
}

/* A slot pressed during a frame that runs no tick still applies, once */
TEST(test_game_weapon_slot_impulse_once) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    inventory_give_weapon(&g.player.inv, WEAPON_SHOTGUN);

    PlayerCmd two = {.weapon_slot = 2};
    PlayerCmd idle = {0};
    game_update(&g, &two, GAME_TICK_DT * 0.5f);
    ASSERT_EQ_INT(weapon_target(&g.weapon), WEAPON_PISTOL); /* no tick yet */
    game_update(&g, &idle, GAME_TICK_DT * 3.0f);
    ASSERT_EQ_INT(weapon_target(&g.weapon), WEAPON_SHOTGUN);
    ASSERT_EQ_INT(g.pending.weapon_slot, 0);
    game_free(&g);
}

/* Picking up a new weapon switches to it */
TEST(test_game_weapon_pickup_switches) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    Vec3 feet = vec3(g.player.view.pos.x, g.player.foot_y, g.player.view.pos.z);
    entity_add(&g.scene.entities, entity_spawn(ENTITY_PICKUP_WEAPON, feet, "give=shotgun"));

    PlayerCmd idle = {0};
    game_tick(&g, &idle);
    ASSERT(inventory_has_weapon(&g.player.inv, WEAPON_SHOTGUN));
    ASSERT_EQ_INT(weapon_target(&g.weapon), WEAPON_SHOTGUN);
    ASSERT(g.player.inv.ammo[AMMO_SHELLS] > 0);
    game_free(&g);
}

/* A shot adds one temporary light that is gone a few ticks later */
TEST(test_game_muzzle_flash) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    int lights = g.scene.lights.count;
    PlayerCmd fire = {.buttons = BTN_ATTACK};
    PlayerCmd idle = {0};

    game_tick(&g, &fire);
    ASSERT_EQ_INT(g.scene.lights.count, lights + 1);

    run_ticks(&idle, 10);
    ASSERT_EQ_INT(g.scene.lights.count, lights);
    game_free(&g);
}

/* The launcher fires a rocket entity with the runtime material */
TEST(test_game_launcher_fires_rocket) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    inventory_give_weapon(&g.player.inv, WEAPON_LAUNCHER);
    inventory_add(&g.player.inv, AMMO_ROCKETS, 3);
    g.weapon = weapon_create(WEAPON_LAUNCHER);

    PlayerCmd fire = {.buttons = BTN_ATTACK};
    game_tick(&g, &fire);

    int rockets = 0;
    for (int i = 0; i < g.scene.entities.count; i++) {
        const Entity *e = &g.scene.entities.items[i];
        if (e->active && e->type == ENTITY_PROJECTILE) {
            rockets++;
            ASSERT_EQ_INT(e->material_id, g.scene.type_material[ENTITY_PROJECTILE]);
        }
    }
    ASSERT_EQ_INT(rockets, 1);
    ASSERT_EQ_INT(g.player.inv.ammo[AMMO_ROCKETS], 2);
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
    RUN_TEST(test_game_shooting_kills_ghoul);
    RUN_TEST(test_game_exit_ends_level);
    RUN_TEST(test_game_reload_keeps_pose);
    RUN_TEST(test_game_weapon_select);
    RUN_TEST(test_game_weapon_slot_impulse_once);
    RUN_TEST(test_game_weapon_pickup_switches);
    RUN_TEST(test_game_muzzle_flash);
    RUN_TEST(test_game_launcher_fires_rocket);
    TEST_REPORT();
}
