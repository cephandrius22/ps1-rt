#include "inventory.h"

static const int ammo_max[AMMO_TYPE_COUNT] = {
    [AMMO_NONE]    = 0,
    [AMMO_BULLETS] = AMMO_BULLETS_MAX,
};

Inventory inventory_create(void) {
    Inventory inv = {0};
    inv.ammo[AMMO_BULLETS] = AMMO_BULLETS_START;
    return inv;
}

int inventory_add(Inventory *inv, AmmoType type, int amount) {
    if (type <= AMMO_NONE || type >= AMMO_TYPE_COUNT || amount <= 0) {
        return 0;
    }

    int room = ammo_max[type] - inv->ammo[type];
    int added = amount < room ? amount : room;
    inv->ammo[type] += added;
    return added;
}

bool inventory_has(const Inventory *inv, AmmoType type, int amount) {
    if (type == AMMO_NONE) {
        return true;
    }
    return inv->ammo[type] >= amount;
}

void inventory_take(Inventory *inv, AmmoType type, int amount) {
    if (type == AMMO_NONE) {
        return;
    }
    inv->ammo[type] -= amount;
}
