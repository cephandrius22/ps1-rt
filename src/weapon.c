#include "weapon.h"

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

/* Draw a simple gun shape in lower-right of screen */
void weapon_draw_viewmodel(uint32_t *pixels, int w, int h, const Weapon *wpn) {
    /* Barrel offset kicks up when firing */
    int kick_offset = (int)(wpn->kick * 200.0f);

    /* Gun body: rectangle in lower-right */
    int gx = w * 3 / 4 - 10;
    int gy = h - 40 - kick_offset;
    int gw = 30;
    int gh = 40;

    uint32_t body_color = 0xFF444444;  /* dark gray */
    uint32_t barrel_color = 0xFF333333;

    /* Body */
    for (int dy = 0; dy < gh; dy++) {
        for (int dx = 0; dx < gw; dx++) {
            int px = gx + dx;
            int py = gy + dy;
            if (px >= 0 && px < w && py >= 0 && py < h)
                pixels[py * w + px] = body_color;
        }
    }

    /* Barrel: narrower rectangle above body */
    int bx = gx + 8;
    int by = gy - 15;
    int bw = 14;
    int bh = 18;
    for (int dy = 0; dy < bh; dy++) {
        for (int dx = 0; dx < bw; dx++) {
            int px = bx + dx;
            int py = by + dy;
            if (px >= 0 && px < w && py >= 0 && py < h)
                pixels[py * w + px] = barrel_color;
        }
    }

    /* Muzzle flash when just fired */
    if (wpn->fired_this_frame) {
        uint32_t flash_color = 0xFFFFDD44;
        int fx = bx + bw / 2;
        int fy = by - 4;
        for (int dy = -4; dy <= 4; dy++) {
            for (int dx = -4; dx <= 4; dx++) {
                if (dx * dx + dy * dy <= 16) {
                    int px = fx + dx;
                    int py = fy + dy;
                    if (px >= 0 && px < w && py >= 0 && py < h)
                        pixels[py * w + px] = flash_color;
                }
            }
        }
    }
}
