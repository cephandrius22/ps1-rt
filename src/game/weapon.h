/*
 * weapon.h — Hitscan weapons driven by a WeaponDef table.
 *
 * Stats live in data (weapon_types.c), not code: a shotgun is a row with
 * more pellets and spread, not a new code path.
 *
 *   PlayerCmd ──> weapon_update() ──> N pellet rays ──> EV_WEAPON_FIRED
 *                                                   └─> EV_WEAPON_IMPACT per hit
 *                                                       (amount = damage)
 */
#ifndef WEAPON_H
#define WEAPON_H

#include "camera.h"
#include "cmd.h"
#include "event.h"
#include <stdint.h>

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
} WeaponDef;

typedef struct {
    WeaponType type;
    float cooldown_timer;
    float kick;             /* current recoil offset */
    bool fired_this_frame;
    HitRecord last_hit;     /* center pellet of last shot */
} Weapon;

const WeaponDef *weapon_def(WeaponType type);

Weapon weapon_create(WeaponType type);

/* Ray for pellet i of a shot (0 = center). Exposed for tests and fx. */
Ray weapon_pellet_ray(const Camera *cam, const WeaponDef *def, int i);

void weapon_update(Weapon *w, const PlayerCmd *cmd, const Camera *cam,
                   const struct Scene *scene, float dt, EventQueue *ev);

/* Draw viewmodel onto framebuffer */
void weapon_draw_viewmodel(uint32_t *pixels, int w, int h, const Weapon *w_);

#endif
