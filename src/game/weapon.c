#include "weapon.h"
#include "scene.h"

Weapon weapon_create(void) {
    Weapon w = {0};
    w.last_hit.hit = false;
    return w;
}

void weapon_update(Weapon *w, const InputState *input, const Camera *cam,
                   const BVH *world, float dt) {
    w->fired_this_frame = false;

    /* Recover recoil */
    if (w->kick > 0) {
        w->kick -= WEAPON_KICK_RETURN * dt;
        if (w->kick < 0) w->kick = 0;
    }

    /* Cooldown */
    if (w->cooldown_timer > 0) {
        w->cooldown_timer -= dt;
        if (w->cooldown_timer < 0) w->cooldown_timer = 0;
    }

    /* Fire */
    if (input->shoot && w->cooldown_timer <= 0) {
        w->cooldown_timer = WEAPON_COOLDOWN;
        w->kick = WEAPON_KICK;
        w->fired_this_frame = true;

        /* Hitscan: cast ray from camera center */
        Ray shot = camera_ray(cam, SCREEN_W / 2, SCREEN_H / 2);
        if (world) {
            w->last_hit = bvh_intersect(world, shot, 0.0f, WEAPON_RANGE);
        } else {
            w->last_hit.hit = false;
        }
    }
}

void weapon_update_scene(Weapon *w, const InputState *input, const Camera *cam,
                         const struct Scene *scene, float dt) {
    w->fired_this_frame = false;

    if (w->kick > 0) {
        w->kick -= WEAPON_KICK_RETURN * dt;
        if (w->kick < 0) w->kick = 0;
    }

    if (w->cooldown_timer > 0) {
        w->cooldown_timer -= dt;
        if (w->cooldown_timer < 0) w->cooldown_timer = 0;
    }

    if (input->shoot && w->cooldown_timer <= 0) {
        w->cooldown_timer = WEAPON_COOLDOWN;
        w->kick = WEAPON_KICK;
        w->fired_this_frame = true;

        Ray shot = camera_ray(cam, SCREEN_W / 2, SCREEN_H / 2);
        w->last_hit = scene_intersect(scene, shot, 0.0f, WEAPON_RANGE);
    }
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
