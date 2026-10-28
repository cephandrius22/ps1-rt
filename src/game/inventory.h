/*
 * inventory.h — Ammo pools and owned weapons carried by an actor.
 *
 * Weapons draw from a pool by AmmoType; pickups refill it. Kept apart
 * from player.h and weapon.h so either can use it without the other —
 * which is why owned weapons are a bitmask of plain ints, not WeaponType
 * (weapon.h includes this header). Bit n = WeaponType n.
 */
#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    AMMO_NONE,    /* weapon needs no ammo (melee, enemy attacks) */
    AMMO_BULLETS,
    AMMO_SHELLS,
    AMMO_ROCKETS,
    AMMO_TYPE_COUNT
} AmmoType;

#define AMMO_BULLETS_START 50
#define AMMO_BULLETS_MAX   200
#define AMMO_SHELLS_MAX    50
#define AMMO_ROCKETS_MAX   25

#define INVENTORY_START_WEAPON 0 /* the pistol */

typedef struct {
    int ammo[AMMO_TYPE_COUNT];
    uint32_t weapons; /* owned: bit n = WeaponType n */
} Inventory;

/* Starting loadout */
Inventory inventory_create(void);

/* Add up to the type's cap. Returns how much was actually added. */
int inventory_add(Inventory *inv, AmmoType type, int amount);

bool inventory_has(const Inventory *inv, AmmoType type, int amount);

/* Remove ammo; caller checks inventory_has() first */
void inventory_take(Inventory *inv, AmmoType type, int amount);

/* True if the pool is at its cap (pickups of it are refused) */
bool inventory_full(const Inventory *inv, AmmoType type);

/* Level-file name → type: "bullets", "shells", "rockets"; AMMO_NONE if unknown */
AmmoType inventory_ammo_by_name(const char *name);

/* weapon is a WeaponType value (plain int to avoid including weapon.h) */
void inventory_give_weapon(Inventory *inv, int weapon);
bool inventory_has_weapon(const Inventory *inv, int weapon);

#endif
