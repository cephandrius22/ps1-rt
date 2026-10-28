#include "test_framework.h"
#include <string.h>
#include "inventory.h"

TEST(test_inventory_start) {
    Inventory inv = inventory_create();
    ASSERT_EQ_INT(inv.ammo[AMMO_BULLETS], AMMO_BULLETS_START);
}

TEST(test_inventory_add_caps) {
    Inventory inv = inventory_create();
    int room = AMMO_BULLETS_MAX - AMMO_BULLETS_START;

    ASSERT_EQ_INT(inventory_add(&inv, AMMO_BULLETS, room + 50), room);
    ASSERT_EQ_INT(inv.ammo[AMMO_BULLETS], AMMO_BULLETS_MAX);
    ASSERT_EQ_INT(inventory_add(&inv, AMMO_BULLETS, 10), 0);
}

TEST(test_inventory_has_and_take) {
    Inventory inv = {0};
    inv.ammo[AMMO_BULLETS] = 2;

    ASSERT(inventory_has(&inv, AMMO_BULLETS, 2));
    ASSERT(!inventory_has(&inv, AMMO_BULLETS, 3));
    inventory_take(&inv, AMMO_BULLETS, 2);
    ASSERT_EQ_INT(inv.ammo[AMMO_BULLETS], 0);
}

TEST(test_inventory_none_is_unlimited) {
    Inventory inv = {0};
    ASSERT(inventory_has(&inv, AMMO_NONE, 100));
    inventory_take(&inv, AMMO_NONE, 100);
    ASSERT_EQ_INT(inventory_add(&inv, AMMO_NONE, 5), 0);
}

TEST(test_inventory_ammo_names) {
    ASSERT_EQ_INT(inventory_ammo_by_name("bullets"), AMMO_BULLETS);
    ASSERT_EQ_INT(inventory_ammo_by_name("shells"), AMMO_SHELLS);
    ASSERT_EQ_INT(inventory_ammo_by_name("plasma"), AMMO_NONE);
}

TEST(test_inventory_weapons) {
    Inventory inv = inventory_create();
    ASSERT(inventory_has_weapon(&inv, INVENTORY_START_WEAPON));
    ASSERT(!inventory_has_weapon(&inv, 1));
    inventory_give_weapon(&inv, 1);
    ASSERT(inventory_has_weapon(&inv, 1));
}

TEST(test_inventory_keycards) {
    Inventory inv = inventory_create();
    ASSERT_EQ_INT(keycard_by_name("blue"), KEYCARD_BLUE);
    ASSERT_EQ_INT(keycard_by_name("green"), KEYCARD_NONE);
    ASSERT(strcmp(keycard_name(KEYCARD_YELLOW), "yellow") == 0);

    ASSERT(!inventory_has_keycard(&inv, KEYCARD_RED));
    inventory_give_keycard(&inv, KEYCARD_RED);
    ASSERT(inventory_has_keycard(&inv, KEYCARD_RED));
    ASSERT(!inventory_has_keycard(&inv, KEYCARD_BLUE));
    ASSERT(!inventory_has_keycard(&inv, KEYCARD_NONE));
}

TEST(test_inventory_full) {
    Inventory inv = {0};
    ASSERT(!inventory_full(&inv, AMMO_SHELLS));
    inventory_add(&inv, AMMO_SHELLS, AMMO_SHELLS_MAX);
    ASSERT(inventory_full(&inv, AMMO_SHELLS));
    ASSERT(inventory_full(&inv, AMMO_NONE)); /* nothing to pick up */
}

int main(void) {
    TEST_SUITE("inventory");
    RUN_TEST(test_inventory_start);
    RUN_TEST(test_inventory_add_caps);
    RUN_TEST(test_inventory_has_and_take);
    RUN_TEST(test_inventory_none_is_unlimited);
    RUN_TEST(test_inventory_ammo_names);
    RUN_TEST(test_inventory_weapons);
    RUN_TEST(test_inventory_keycards);
    RUN_TEST(test_inventory_full);
    TEST_REPORT();
}
