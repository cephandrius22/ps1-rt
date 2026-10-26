/*
 * weapon.h — Hitscan weapon with cooldown, crosshair, and viewmodel.
 */
#ifndef WEAPON_H
#define WEAPON_H

#include "camera.h"
#include "bvh.h"
#include "input.h"
#include <stdint.h>

struct Scene;

#define WEAPON_COOLDOWN    0.2f   /* seconds between shots */
#define WEAPON_DAMAGE      25.0f
#define WEAPON_RANGE       100.0f
#define WEAPON_KICK        0.04f  /* pitch recoil per shot */
#define WEAPON_KICK_RETURN 0.3f   /* recoil recovery per second */

typedef struct {
    float cooldown_timer;
    float kick;             /* current recoil offset */
    bool fired_this_frame;
    HitRecord last_hit;     /* result of last shot */
} Weapon;

Weapon weapon_create(void);
void weapon_update(Weapon *w, const InputState *input, const Camera *cam,
                   const BVH *world, float dt);
void weapon_update_scene(Weapon *w, const InputState *input, const Camera *cam,
                         const struct Scene *scene, float dt);

/* Draw crosshair and viewmodel onto framebuffer */
void weapon_draw_crosshair(uint32_t *pixels, int w, int h);
void weapon_draw_viewmodel(uint32_t *pixels, int w, int h, const Weapon *w_);

#endif
