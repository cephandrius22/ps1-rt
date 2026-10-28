#include "weapon.h"
#include "scene.h"
#include <math.h>

#define TWO_PI 6.28318530718f

Weapon weapon_create(WeaponType type) {
    Weapon w = {.type = type, .phase = WEAPON_READY, .pending = type};
    w.last_hit.hit = false;
    return w;
}

WeaponType weapon_target(const Weapon *w) {
    return w->phase == WEAPON_LOWERING ? w->pending : w->type;
}

void weapon_request(Weapon *w, WeaponType type) {
    if (type < 0 || type >= WEAPON_TYPE_COUNT || type == weapon_target(w)) {
        return;
    }

    /* Mid-raise: drop straight back down from where it is */
    float lowered = weapon_lowered(w);
    float half = weapon_def(w->type)->switch_time * 0.5f;
    w->pending = type;
    w->phase = WEAPON_LOWERING;
    w->phase_time = lowered * half;
}

float weapon_lowered(const Weapon *w) {
    float half = weapon_def(w->type)->switch_time * 0.5f;
    if (half <= 0) {
        return 0;
    }
    float k = fminf(w->phase_time / half, 1.0f);
    switch (w->phase) {
    case WEAPON_READY:    return 0;
    case WEAPON_LOWERING: return k;
    case WEAPON_RAISING:  return 1 - k;
    }
    return 0;
}

/* Advance a switch in progress. Returns true once the weapon is ready. */
static bool tick_switch(Weapon *w, float dt) {
    if (w->phase == WEAPON_READY) {
        return true;
    }

    w->phase_time += dt;
    float half = weapon_def(w->type)->switch_time * 0.5f;
    if (w->phase_time < half) {
        return false;
    }

    /* Lowered: swap type, raise the new one (with its own timing) */
    if (w->phase == WEAPON_LOWERING) {
        w->type = w->pending;
        w->phase = WEAPON_RAISING;
        w->phase_time = 0;
        w->cooldown_timer = 0;
        return false;
    }
    w->phase = WEAPON_READY;
    w->phase_time = 0;
    return true;
}

/* Recover recoil and count down cooldown. Returns true if a shot may fire. */
static bool tick_ready(Weapon *w, const WeaponDef *def, const PlayerCmd *cmd, float dt) {
    w->fired_this_tick = false;
    w->launch_this_tick = false;

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

    bool ready = tick_switch(w, dt);
    return ready && cmd_has(cmd, BTN_ATTACK) && w->cooldown_timer <= 0;
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
    w->fired_this_tick = true;
    events_push(ev, (GameEvent){.type = EV_WEAPON_FIRED, .pos = aim.pos,
                                .entity = ENTITY_INDEX_NONE});

    if (def->projectile != PROJECTILE_NONE) {
        w->launch_this_tick = true;
        w->launch = weapon_pellet_ray(aim, def, 0);
        return;
    }

    /* One impact event per pellet that hits; damage rides in amount */
    for (int i = 0; i < def->pellets; i++) {
        HitRecord hit = scene_trace(scene, weapon_pellet_ray(aim, def, i), 0.0f, def->range,
                                    TRACE_SOLID);
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

/* Attack with an empty pool: click once per cooldown, no shot */
static void dry_fire(Weapon *w, Pose aim, EventQueue *ev) {
    w->cooldown_timer = weapon_def(w->type)->cooldown;
    events_push(ev, (GameEvent){.type = EV_WEAPON_EMPTY, .pos = aim.pos,
                                .entity = ENTITY_INDEX_NONE});
}

void weapon_update(Weapon *w, const PlayerCmd *cmd, Pose aim,
                   const Scene *scene, float dt, Inventory *inv,
                   EventQueue *ev) {
    const WeaponDef *def = weapon_def(w->type);
    if (!tick_ready(w, def, cmd, dt)) {
        return;
    }

    if (!inventory_has(inv, def->ammo, def->ammo_per_shot)) {
        dry_fire(w, aim, ev);
        return;
    }
    inventory_take(inv, def->ammo, def->ammo_per_shot);
    fire(w, aim, scene, ev);
}
