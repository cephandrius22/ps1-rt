#include "weapon.h"
#include "scene.h"
#include <math.h>

#define TWO_PI 6.28318530718f

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
Ray weapon_pellet_ray(Pose aim, const WeaponDef *def, int i) {
    float right = 0;
    float up = 0;
    if (i > 0) {
        float angle = TWO_PI * (float)(i - 1) / (float)(def->pellets - 1);
        right = cosf(angle) * def->spread;
        up = sinf(angle) * def->spread;
    }

    Vec3 local = vec3(tanf(right), tanf(up), -1.0f);
    return (Ray){aim.pos, pose_to_world_dir(aim, local)};
}

static void fire(Weapon *w, Pose aim, const Scene *scene, EventQueue *ev) {
    const WeaponDef *def = weapon_def(w->type);

    w->cooldown_timer = def->cooldown;
    w->kick = def->kick;
    w->fired_this_frame = true;
    events_push(ev, (GameEvent){.type = EV_WEAPON_FIRED, .pos = aim.pos,
                                .entity = ENTITY_INDEX_NONE});

    /* One impact event per pellet that hits; damage rides in amount */
    for (int i = 0; i < def->pellets; i++) {
        HitRecord hit = scene_intersect(scene, weapon_pellet_ray(aim, def, i), 0.0f, def->range);
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

void weapon_update(Weapon *w, const PlayerCmd *cmd, Pose aim,
                   const Scene *scene, float dt, EventQueue *ev) {
    if (!tick_ready(w, weapon_def(w->type), cmd, dt)) {
        return;
    }
    fire(w, aim, scene, ev);
}

void weapon_draw_viewmodel(uint32_t *pixels, int w, int h, const Weapon *wpn) {
    (void)pixels; (void)w; (void)h; (void)wpn;
}
