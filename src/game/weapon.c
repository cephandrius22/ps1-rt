#include "weapon.h"
#include "scene.h"

Weapon weapon_create(void) {
    Weapon w = {0};
    w.last_hit.hit = false;
    return w;
}

/* Recover recoil and count down cooldown. Returns true if a shot may fire. */
static bool tick_ready(Weapon *w, const PlayerCmd *cmd, float dt) {
    w->fired_this_frame = false;

    if (w->kick > 0) {
        w->kick -= WEAPON_KICK_RETURN * dt;
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

/* Start a shot: arm cooldown and recoil, return the hitscan ray */
static Ray begin_shot(Weapon *w, const Camera *cam, EventQueue *ev) {
    w->cooldown_timer = WEAPON_COOLDOWN;
    w->kick = WEAPON_KICK;
    w->fired_this_frame = true;

    events_push(ev, (GameEvent){.type = EV_WEAPON_FIRED, .pos = cam->position,
                                .entity = ENTITY_INDEX_NONE});
    return camera_ray(cam, SCREEN_W / 2, SCREEN_H / 2);
}

/* Record the shot result; announce an impact if something was hit */
static void end_shot(Weapon *w, HitRecord hit, EventQueue *ev) {
    w->last_hit = hit;
    if (!hit.hit) {
        return;
    }
    events_push(ev, (GameEvent){.type = EV_WEAPON_IMPACT, .pos = hit.point,
                                .entity = entity_hit(&hit)});
}

void weapon_update(Weapon *w, const PlayerCmd *cmd, const Camera *cam,
                   const BVH *world, float dt, EventQueue *ev) {
    if (!tick_ready(w, cmd, dt)) {
        return;
    }

    Ray shot = begin_shot(w, cam, ev);
    HitRecord miss = {.hit = false};
    end_shot(w, world ? bvh_intersect(world, shot, 0.0f, WEAPON_RANGE) : miss, ev);
}

void weapon_update_scene(Weapon *w, const PlayerCmd *cmd, const Camera *cam,
                         const struct Scene *scene, float dt, EventQueue *ev) {
    if (!tick_ready(w, cmd, dt)) {
        return;
    }

    Ray shot = begin_shot(w, cam, ev);
    end_shot(w, scene_intersect(scene, shot, 0.0f, WEAPON_RANGE), ev);
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
