#include "test_framework.h"
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

int main(void) {
    TEST_SUITE("inventory");
    RUN_TEST(test_inventory_start);
    RUN_TEST(test_inventory_add_caps);
    RUN_TEST(test_inventory_has_and_take);
    RUN_TEST(test_inventory_none_is_unlimited);
    TEST_REPORT();
}
