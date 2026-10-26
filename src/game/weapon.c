#include "weapon.h"
#include "scene.h"
#include <math.h>

#define TWO_PI 6.28318530718f

/* Ray-cast backend, so one firing routine serves BVH and full-scene paths */
typedef HitRecord (*TraceFn)(const void *world, Ray ray, float t_min, float t_max);

static HitRecord trace_bvh(const void *world, Ray ray, float t_min, float t_max) {
    if (!world) {
        return (HitRecord){.hit = false};
    }
    return bvh_intersect(world, ray, t_min, t_max);
}

static HitRecord trace_scene(const void *world, Ray ray, float t_min, float t_max) {
    return scene_intersect(world, ray, t_min, t_max);
}

Weapon weapon_create(WeaponType type) {
    Weapon w = {.type = type};
    w.last_hit.hit = false;
    return w;
}

/* Recover recoil and count down cooldown. Returns true if a shot may fire. */
static bool tick_ready(Weapon *w, const WeaponDef *def, const PlayerCmd *cmd, float dt) {
    w->fired_this_frame = false;

    if (w->kick > 0) {
        w->kick -= def->kick_return * dt;
        if (w->kick < 0) {
            w->kick = 0;
        }
    }

    if (w->cooldown_timer > 0) {
        w->cooldown_timer -= dt;
        if (w->cooldown_timer < 0) {
            w->cooldown_timer = 0;
        }
    }

    return cmd_has(cmd, BTN_ATTACK) && w->cooldown_timer <= 0;
}

/* Pellet i's ray. Deterministic pattern: pellet 0 dead center, the rest
 * evenly spaced on a ring of radius `spread`.
 *
 *        o
 *     o  x  o     x = pellet 0, o = pellets 1..n-1
 *        o
 */
Ray weapon_pellet_ray(const Camera *cam, const WeaponDef *def, int i) {
    if (i == 0) {
        return camera_ray_offset(cam, 0, 0);
    }
    float angle = TWO_PI * (float)(i - 1) / (float)(def->pellets - 1);
    return camera_ray_offset(cam, cosf(angle) * def->spread, sinf(angle) * def->spread);
}

static void fire(Weapon *w, const Camera *cam, TraceFn trace, const void *world,
                 EventQueue *ev) {
    const WeaponDef *def = weapon_def(w->type);

    w->cooldown_timer = def->cooldown;
    w->kick = def->kick;
    w->fired_this_frame = true;
    events_push(ev, (GameEvent){.type = EV_WEAPON_FIRED, .pos = cam->position,
                                .entity = ENTITY_INDEX_NONE});

    /* One impact event per pellet that hits; damage rides in amount */
    for (int i = 0; i < def->pellets; i++) {
        HitRecord hit = trace(world, weapon_pellet_ray(cam, def, i), 0.0f, def->range);
        if (i == 0) {
            w->last_hit = hit;
        }
        if (!hit.hit) {
            continue;
        }
        events_push(ev, (GameEvent){.type = EV_WEAPON_IMPACT, .pos = hit.point,
                                    .entity = entity_hit(&hit), .amount = def->damage});
    }
}

void weapon_update(Weapon *w, const PlayerCmd *cmd, const Camera *cam,
                   const BVH *world, float dt, EventQueue *ev) {
    if (!tick_ready(w, weapon_def(w->type), cmd, dt)) {
        return;
    }
    fire(w, cam, trace_bvh, world, ev);
}

void weapon_update_scene(Weapon *w, const PlayerCmd *cmd, const Camera *cam,
                         const struct Scene *scene, float dt, EventQueue *ev) {
    if (!tick_ready(w, weapon_def(w->type), cmd, dt)) {
        return;
    }
    fire(w, cam, trace_scene, scene, ev);
}

/* Draw a small + crosshair at screen center */
void weapon_draw_crosshair(uint32_t *pixels, int w, int h) {
    int cx = w / 2;
    int cy = h / 2;
    uint32_t color = 0xFFFFFFFF; /* white */
    int size = 3;

    for (int i = -size; i <= size; i++) {
        int px, py;
        /* Horizontal */
        px = cx + i; py = cy;
        if (px >= 0 && px < w && py >= 0 && py < h)
            pixels[py * w + px] = color;
        /* Vertical */
        px = cx; py = cy + i;
        if (px >= 0 && px < w && py >= 0 && py < h)
            pixels[py * w + px] = color;
    }
}

void weapon_draw_viewmodel(uint32_t *pixels, int w, int h, const Weapon *wpn) {
    (void)pixels; (void)w; (void)h; (void)wpn;
}
