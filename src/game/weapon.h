/*
 * weapon.h — Hitscan weapons driven by a WeaponDef table.
 *
 * Stats live in data (weapon_types.c), not code: a shotgun is a row with
 * more pellets and spread, not a new code path.
 *
 *   PlayerCmd ──> weapon_update() ──> N pellet rays ──> EV_WEAPON_FIRED
 *                    │                              └─> EV_WEAPON_IMPACT per hit
 *                    │                                  (amount = damage)
 *                    └─ no ammo in Inventory ──> EV_WEAPON_EMPTY (dry fire)
 */
#ifndef WEAPON_H
#define WEAPON_H

#include "math3d.h"
#include "cmd.h"
#include "event.h"
#include "inventory.h"

struct Scene;

typedef enum {
    WEAPON_PISTOL,
    WEAPON_TYPE_COUNT
} WeaponType;

typedef struct {
    const char *name;
    float damage;      /* per pellet hit */
    float cooldown;    /* seconds between shots */
    float range;       /* max hitscan distance */
    int   pellets;     /* rays per shot; 1 = single bullet */
    float spread;      /* radians off-center for outer pellets */
    float kick;        /* pitch recoil per shot (radians) */
    float kick_return; /* recoil recovery per second */
    AmmoType ammo;     /* pool drawn from; AMMO_NONE = unlimited */
    int ammo_per_shot;
} WeaponDef;

typedef struct {
    WeaponType type;
    float cooldown_timer;
    float kick;             /* current recoil offset */
    bool fired_this_tick;
    HitRecord last_hit;     /* center pellet of last shot */
} Weapon;

const WeaponDef *weapon_def(WeaponType type);

Weapon weapon_create(WeaponType type);

/* Ray for pellet i of a shot (0 = center). Exposed for tests and fx. */
Ray weapon_pellet_ray(Pose aim, const WeaponDef *def, int i);

/* One tick: cooldown/recoil, and fire along aim if attack is held and
 * inv holds enough ammo */
void weapon_update(Weapon *w, const PlayerCmd *cmd, Pose aim,
                   const struct Scene *scene, float dt, Inventory *inv,
                   EventQueue *ev);

#endif
