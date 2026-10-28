/*
 * weapon.h — Hitscan weapons driven by a WeaponDef table.
 *
 * Stats live in data (weapon_types.c), not code: a shotgun is a row with
 * more pellets and spread, not a new code path.
 *
 *   PlayerCmd ──> weapon_update() ──> N pellet rays ──> EV_WEAPON_FIRED
 *                    │                              └─> EV_WEAPON_IMPACT per hit
 *                    │                                  (amount = damage)
 *                    ├─ projectile weapon ──> launch request (game.c fires it)
 *                    └─ no ammo in Inventory ──> EV_WEAPON_EMPTY (dry fire)
 *
 * Switching lowers the current weapon, swaps type at the midpoint, then
 * raises the new one; only READY fires:
 *
 *   READY ──request──> LOWERING ──switch_time/2──> RAISING ──switch_time/2──> READY
 *                                (type = pending)
 */
#ifndef WEAPON_H
#define WEAPON_H

#include "math3d.h"
#include "cmd.h"
#include "event.h"
#include "inventory.h"
#include "projectile.h"

struct Scene;

/* Order = selection slot: 1 = pistol, 2 = shotgun, ... */
typedef enum {
    WEAPON_PISTOL,
    WEAPON_SHOTGUN,
    WEAPON_LAUNCHER,
    WEAPON_TYPE_COUNT
} WeaponType;

typedef enum {
    WEAPON_READY,
    WEAPON_LOWERING,
    WEAPON_RAISING,
} WeaponPhase;

typedef struct {
    const char *name;
    float damage;      /* per pellet hit */
    float cooldown;    /* seconds between shots */
    float range;       /* max hitscan distance */
    int   pellets;     /* rays per shot; 1 = single bullet */
    float spread;      /* radians off-center for outer pellets */
    float kick;        /* pitch recoil per shot (radians) */
    float kick_return; /* recoil recovery per second */
    const char *view_texture; /* viewmodel sheet (proctex name) */
    AmmoType ammo;     /* pool drawn from; AMMO_NONE = unlimited */
    int ammo_per_shot;
    float switch_time; /* seconds to lower the old + raise this one */

    /* Muzzle flash: a temporary light in front of the shooter */
    Vec3 flash_color;
    float flash_intensity;
    float flash_radius;  /* meters */
    float flash_time;    /* seconds; fades over this */

    float shake;         /* camera jolt per shot, radians (presentation) */
    float noise;         /* walking distance (m) at which idle enemies hear a shot */

    ProjectileKind projectile; /* PROJECTILE_NONE = hitscan */
} WeaponDef;

typedef struct {
    WeaponType type;
    float cooldown_timer;
    float kick;             /* current recoil offset */
    bool fired_this_tick;
    HitRecord last_hit;     /* center pellet of last shot */
    WeaponPhase phase;
    float phase_time;       /* seconds into the current phase */
    WeaponType pending;     /* type to switch to once lowered */

    /* Projectile weapons don't trace: they ask game.c to launch this ray
     * (weapon_update only gets a read-only scene) */
    bool launch_this_tick;
    Ray launch;
} Weapon;

/* Stats row for a type (weapon_types.c); out-of-range falls back to pistol */
const WeaponDef *weapon_def(WeaponType type);

/* Type by level/config name ("shotgun"); WEAPON_TYPE_COUNT if unknown */
WeaponType weapon_type_by_name(const char *name);

/* Ready to fire: no cooldown, no recoil */
Weapon weapon_create(WeaponType type);

/* Start switching to type (no-op if already holding or switching to it) */
void weapon_request(Weapon *w, WeaponType type);

/* How far the weapon is lowered: 0 = ready, 1 = fully down (mid-switch) */
float weapon_lowered(const Weapon *w);

/* The type being held or switched to (what a new request compares against) */
WeaponType weapon_target(const Weapon *w);

/* Ray for pellet i of a shot (0 = center). Exposed for tests and fx. */
Ray weapon_pellet_ray(Pose aim, const WeaponDef *def, int i);

/* One tick: cooldown/recoil, and fire along aim if attack is held and
 * inv holds enough ammo */
void weapon_update(Weapon *w, const PlayerCmd *cmd, Pose aim,
                   const struct Scene *scene, float dt, Inventory *inv,
                   EventQueue *ev);

#endif
