/*
 * weapon_types.c — WeaponDef table. Add a weapon = enum value + one row.
 */
#include "weapon.h"

static const WeaponDef weapon_defs[WEAPON_TYPE_COUNT] = {
    [WEAPON_PISTOL] = {
        .name        = "pistol",
        .damage      = 25.0f,
        .cooldown    = 0.2f,
        .range       = 100.0f,
        .pellets     = 1,
        .spread      = 0.0f,
        .kick        = 0.04f,
        .kick_return = 0.3f,
    },
};

const WeaponDef *weapon_def(WeaponType type) {
    if (type < 0 || type >= WEAPON_TYPE_COUNT) {
        return &weapon_defs[WEAPON_PISTOL];
    }
    return &weapon_defs[type];
}
