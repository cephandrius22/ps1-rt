/*
 * inventory.h — Ammo pools carried by an actor.
 *
 * Weapons draw from a pool by AmmoType; pickups refill it. Kept apart
 * from player.h and weapon.h so either can use it without the other.
 */
#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdbool.h>

typedef enum {
    AMMO_NONE,    /* weapon needs no ammo (melee, enemy attacks) */
    AMMO_BULLETS,
    AMMO_TYPE_COUNT
} AmmoType;

#define AMMO_BULLETS_START 50
#define AMMO_BULLETS_MAX   200

typedef struct {
    int ammo[AMMO_TYPE_COUNT];
} Inventory;

/* Starting loadout */
Inventory inventory_create(void);

/* Add up to the type's cap. Returns how much was actually added. */
int inventory_add(Inventory *inv, AmmoType type, int amount);

bool inventory_has(const Inventory *inv, AmmoType type, int amount);

/* Remove ammo; caller checks inventory_has() first */
void inventory_take(Inventory *inv, AmmoType type, int amount);

#endif
