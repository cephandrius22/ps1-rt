#include "test_framework.h"
#include "save.h"
#include <stdio.h>
#include <string.h>

#define TEST_LEVEL "tests/test.level"
#define SAVE_TMP   "build/tests/save_tmp.sav"
#define DOOR       2   /* test.level: health, decor, func_door */

static Game g; /* large: keep off the stack */

/* Everything that changes in play comes back: player, inventory, weapon,
 * entity state (an opened door, a taken pickup), lights, tallies */
TEST(test_save_round_trip) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    g.player.view.pos = vec3(1, 2, 3);
    g.player.health = 42;
    inventory_give_weapon(&g.player.inv, WEAPON_SHOTGUN);
    inventory_add(&g.player.inv, AMMO_SHELLS, 7);
    g.weapon = weapon_create(WEAPON_SHOTGUN);
    g.scene.entities.items[0].active = false;                 /* health taken */
    g.scene.entities.items[DOOR].mover.state = MOVER_OPEN;
    g.scene.entities.items[DOOR].mover.offset = 1.5f;
    g.scene.lights.items[0].off = true;
    g.kills = 3;
    g.map_time = 75;
    ASSERT(game_save(&g, SAVE_TMP));

    /* Scramble, then load */
    g.player.health = 100;
    g.player.view.pos = vec3(0, 0, 0);
    g.scene.entities.items[0].active = true;
    g.scene.lights.items[0].off = false;
    g.kills = 0;
    ASSERT(game_load(&g, SAVE_TMP) == GAME_LOAD_OK);

    ASSERT_NEAR(g.player.health, 42, 1e-6f);
    ASSERT_NEAR(g.player.view.pos.z, 3, 1e-6f);
    ASSERT(inventory_has_weapon(&g.player.inv, WEAPON_SHOTGUN));
    ASSERT_EQ_INT(g.player.inv.ammo[AMMO_SHELLS], 7);
    ASSERT_EQ_INT(g.weapon.type, WEAPON_SHOTGUN);
    ASSERT(!g.scene.entities.items[0].active);
    ASSERT_EQ_INT(g.scene.entities.items[DOOR].mover.state, MOVER_OPEN);
    ASSERT_NEAR(g.scene.entities.items[DOOR].mover.offset, 1.5f, 1e-6f);
    ASSERT(!g.scene.entities.items[DOOR].mover.nav_closed); /* re-marked by logic */
    ASSERT(g.scene.lights.items[0].off);
    ASSERT_EQ_INT(g.kills, 3);
    ASSERT_NEAR(g.map_time, 75, 1e-6f);
    ASSERT(g.mode == GAME_PLAYING);

    /* It keeps playing: a tick runs with the restored world */
    PlayerCmd idle = {0};
    game_tick(&g, &idle);
    game_free(&g);
}

TEST(test_save_peek_and_refusals) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    g.map_time = 12;
    ASSERT(game_save(&g, SAVE_TMP));
    SaveInfo info;
    ASSERT(save_peek(SAVE_TMP, &info));
    ASSERT(strcmp(info.map_title, "test") == 0);
    ASSERT_NEAR(info.map_time, 12, 1e-6f);

    /* Damaged file: refused, game untouched */
    FILE *f = fopen(SAVE_TMP, "r+b");
    fputs("garbage", f);
    fclose(f);
    g.player.health = 55;
    ASSERT(!save_peek(SAVE_TMP, &info));
    ASSERT(game_load(&g, SAVE_TMP) == GAME_LOAD_FAILED);
    ASSERT_NEAR(g.player.health, 55, 1e-6f);
    ASSERT(game_load(&g, "build/tests/no_such.sav") == GAME_LOAD_FAILED);

    /* No saving while dead */
    g.mode = GAME_DEAD;
    ASSERT(!game_save(&g, SAVE_TMP));
    remove(SAVE_TMP);
    game_free(&g);
}

/* A save whose map has since changed loads the map fresh, with a notice */
TEST(test_save_map_changed) {
    ASSERT(game_init(&g, TEST_LEVEL) == GAME_LOAD_OK);
    g.scene.entities.items[0].type = ENTITY_GHOUL;  /* pretend the level differed */
    g.player.health = 42;
    ASSERT(game_save(&g, SAVE_TMP));
    ASSERT(game_load(&g, SAVE_TMP) == GAME_LOAD_OK);
    ASSERT_EQ_INT(g.scene.entities.items[0].type, ENTITY_PICKUP_HEALTH);
    ASSERT(game_message(&g) && strstr(game_message(&g), "changed"));
    remove(SAVE_TMP);
    game_free(&g);
}

int main(void) {
    TEST_SUITE("save");
    RUN_TEST(test_save_round_trip);
    RUN_TEST(test_save_peek_and_refusals);
    RUN_TEST(test_save_map_changed);
    TEST_REPORT();
}
