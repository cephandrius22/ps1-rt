#include "inventory.h"
#include <string.h>

static const int ammo_max[AMMO_TYPE_COUNT] = {
    [AMMO_NONE]    = 0,
    [AMMO_BULLETS] = AMMO_BULLETS_MAX,
    [AMMO_SHELLS]  = AMMO_SHELLS_MAX,
    [AMMO_ROCKETS] = AMMO_ROCKETS_MAX,
};

static const char *ammo_names[AMMO_TYPE_COUNT] = {
    [AMMO_NONE]    = "none",
    [AMMO_BULLETS] = "bullets",
    [AMMO_SHELLS]  = "shells",
    [AMMO_ROCKETS] = "rockets",
};

Inventory inventory_create(void) {
    Inventory inv = {0};
    inv.ammo[AMMO_BULLETS] = AMMO_BULLETS_START;
    inventory_give_weapon(&inv, INVENTORY_START_WEAPON);
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

bool inventory_full(const Inventory *inv, AmmoType type) {
    if (type <= AMMO_NONE || type >= AMMO_TYPE_COUNT) {
        return true;
    }
    return inv->ammo[type] >= ammo_max[type];
}

AmmoType inventory_ammo_by_name(const char *name) {
    for (int t = AMMO_NONE + 1; t < AMMO_TYPE_COUNT; t++) {
        if (strcmp(ammo_names[t], name) == 0) {
            return (AmmoType)t;
        }
    }
    return AMMO_NONE;
}

void inventory_give_weapon(Inventory *inv, int weapon) {
    inv->weapons |= 1u << weapon;
}

bool inventory_has_weapon(const Inventory *inv, int weapon) {
    return (inv->weapons & (1u << weapon)) != 0;
}
