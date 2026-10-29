/*
 * weapon_types.c — WeaponDef table. Add a weapon = enum value + one row.
 */
#include "weapon.h"
#include <string.h>

static const WeaponDef weapon_defs[WEAPON_TYPE_COUNT] = {
    [WEAPON_PISTOL] = {
        .name        = "pistol",
        .view_texture = "view_pistol",
        .damage      = 34.0f,   /* ghoul (60) and spitter (45) in two */
        .cooldown    = 0.2f,
        .range       = 100.0f,
        .pellets     = 1,
        .spread      = 0.0f,
        .kick        = 0.055f,
        .kick_return = 0.3f,
        .ammo        = AMMO_BULLETS,
        .ammo_per_shot = 1,
        .switch_time = 0.4f,
        .flash_color = {1.0f, 0.8f, 0.45f},
        .flash_intensity = 2.8f,
        .flash_radius = 7.0f,
        .flash_time = 0.08f,
        .shake = 0.008f,
        .noise = 25.0f,
    },
    [WEAPON_SHOTGUN] = {
        .name        = "shotgun",
        .view_texture = "view_shotgun",
        .damage      = 14.0f,   /* per pellet: 7 pellets point-blank = 98, a ghoul in one */
        .cooldown    = 0.9f,
        .range       = 60.0f,
        .pellets     = 7,
        .spread      = 0.07f,
        .kick        = 0.13f,
        .kick_return = 0.25f,
        .ammo        = AMMO_SHELLS,
        .ammo_per_shot = 1,
        .switch_time = 0.5f,
        .flash_color = {1.0f, 0.75f, 0.4f},
        .flash_intensity = 3.8f,
        .flash_radius = 11.0f,
        .flash_time = 0.12f,
        .shake = 0.035f,
        .noise = 35.0f,
    },
    [WEAPON_LAUNCHER] = {
        .name        = "launcher",
        .view_texture = "view_launcher",
        .damage      = 0.0f,    /* the rocket carries the damage */
        .cooldown    = 0.8f,
        .range       = 0.0f,
        .pellets     = 1,
        .kick        = 0.1f,
        .kick_return = 0.25f,
        .ammo        = AMMO_ROCKETS,
        .ammo_per_shot = 1,
        .switch_time = 0.6f,
        .flash_color = {1.0f, 0.6f, 0.3f},
        .flash_intensity = 3.4f,
        .flash_radius = 10.0f,
        .flash_time = 0.13f,
        .shake = 0.045f,
        .noise = 35.0f,
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
