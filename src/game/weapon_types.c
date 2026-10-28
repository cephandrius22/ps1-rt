/*
 * weapon_types.c — WeaponDef table. Add a weapon = enum value + one row.
 */
#include "weapon.h"
#include <string.h>

static const WeaponDef weapon_defs[WEAPON_TYPE_COUNT] = {
    [WEAPON_PISTOL] = {
        .name        = "pistol",
        .view_texture = "view_pistol",
        .damage      = 25.0f,
        .cooldown    = 0.2f,
        .range       = 100.0f,
        .pellets     = 1,
        .spread      = 0.0f,
        .kick        = 0.04f,
        .kick_return = 0.3f,
        .ammo        = AMMO_BULLETS,
        .ammo_per_shot = 1,
        .switch_time = 0.4f,
        .flash_color = {1.0f, 0.8f, 0.45f},
        .flash_intensity = 2.0f,
        .flash_radius = 6.0f,
        .flash_time = 0.06f,
        .shake = 0.004f,
    },
    [WEAPON_SHOTGUN] = {
        .name        = "shotgun",
        .view_texture = "view_shotgun",
        .damage      = 10.0f,   /* per pellet: 7 pellets point-blank = 70 */
        .cooldown    = 0.9f,
        .range       = 60.0f,
        .pellets     = 7,
        .spread      = 0.07f,
        .kick        = 0.09f,
        .kick_return = 0.25f,
        .ammo        = AMMO_SHELLS,
        .ammo_per_shot = 1,
        .switch_time = 0.5f,
        .flash_color = {1.0f, 0.75f, 0.4f},
        .flash_intensity = 2.8f,
        .flash_radius = 9.0f,
        .flash_time = 0.09f,
        .shake = 0.02f,
    },
    [WEAPON_LAUNCHER] = {
        .name        = "launcher",
        .view_texture = "view_launcher",
        .damage      = 0.0f,    /* the rocket carries the damage */
        .cooldown    = 0.8f,
        .range       = 0.0f,
        .pellets     = 1,
        .kick        = 0.07f,
        .kick_return = 0.25f,
        .ammo        = AMMO_ROCKETS,
        .ammo_per_shot = 1,
        .switch_time = 0.6f,
        .flash_color = {1.0f, 0.6f, 0.3f},
        .flash_intensity = 2.5f,
        .flash_radius = 8.0f,
        .flash_time = 0.1f,
        .shake = 0.03f,
        .projectile = PROJECTILE_ROCKET,
    },
};

const WeaponDef *weapon_def(WeaponType type) {
    if (type < 0 || type >= WEAPON_TYPE_COUNT) {
        return &weapon_defs[WEAPON_PISTOL];
    }
    return &weapon_defs[type];
}

WeaponType weapon_type_by_name(const char *name) {
    for (int t = 0; t < WEAPON_TYPE_COUNT; t++) {
        if (strcmp(weapon_defs[t].name, name) == 0) {
            return (WeaponType)t;
        }
    }
    return WEAPON_TYPE_COUNT;
}
