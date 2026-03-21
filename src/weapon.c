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

static void draw_rect(uint32_t *pixels, int sw, int sh,
                      int x, int y, int rw, int rh, uint32_t color) {
    for (int dy = 0; dy < rh; dy++) {
        for (int dx = 0; dx < rw; dx++) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < sw && py >= 0 && py < sh)
                pixels[py * sw + px] = color;
        }
    }
}

static void draw_pixel(uint32_t *pixels, int sw, int sh,
                       int x, int y, uint32_t color) {
    if (x >= 0 && x < sw && y >= 0 && y < sh)
        pixels[y * sw + x] = color;
}

/* Draw a pistol viewed from first person, held in the right hand */
void weapon_draw_viewmodel(uint32_t *pixels, int w, int h, const Weapon *wpn) {
    int kick_offset = (int)(wpn->kick * 300.0f);

    /* Anchor: bottom-right of screen, hand holding a pistol */
    int ox = w - 90;
    int oy = h - 10 - kick_offset;

    /* Colors */
    uint32_t grip_dark   = 0xFF2A2A2A;
    uint32_t grip_light  = 0xFF3A3A3A;
    uint32_t grip_tex    = 0xFF222222;
    uint32_t metal_dark  = 0xFF4A4A4A;
    uint32_t metal_mid   = 0xFF5A5A5A;
    uint32_t metal_light = 0xFF6A6A6A;
    uint32_t metal_hi    = 0xFF7A7A7A;
    uint32_t trigger_col = 0xFF505050;
    uint32_t sight_col   = 0xFF3A3A3A;
    uint32_t hand_col    = 0xFFB08060;
    uint32_t hand_shade  = 0xFF906848;

    /* --- Hand / fingers wrapping around grip --- */
    draw_rect(pixels, w, h, ox + 14, oy - 20, 30, 12, hand_col);
    draw_rect(pixels, w, h, ox + 16, oy - 22, 26,  4, hand_col);
    draw_rect(pixels, w, h, ox + 14, oy -  8, 28,  8, hand_shade);
    /* Thumb on left side */
    draw_rect(pixels, w, h, ox + 10, oy - 26, 6, 10, hand_col);
    draw_rect(pixels, w, h, ox + 10, oy - 16, 6,  4, hand_shade);

    /* --- Grip (angled slightly, wider at bottom) --- */
    draw_rect(pixels, w, h, ox + 18, oy - 44, 22, 36, grip_dark);
    draw_rect(pixels, w, h, ox + 20, oy - 42, 18, 32, grip_light);
    /* Grip texture lines */
    for (int i = 0; i < 7; i++) {
        draw_rect(pixels, w, h, ox + 21, oy - 40 + i * 4, 16, 1, grip_tex);
    }
    /* Grip base (wider, palm swell) */
    draw_rect(pixels, w, h, ox + 16, oy - 10, 26,  6, grip_dark);

    /* --- Trigger guard --- */
    draw_rect(pixels, w, h, ox + 14, oy - 44, 6,  2, metal_dark);
    draw_rect(pixels, w, h, ox + 12, oy - 42, 2, 10, metal_dark);
    draw_rect(pixels, w, h, ox + 12, oy - 32, 8,  2, metal_dark);
    /* Trigger */
    draw_rect(pixels, w, h, ox + 16, oy - 40, 3,  6, trigger_col);

    /* --- Slide / upper receiver --- */
    draw_rect(pixels, w, h, ox + 10, oy - 62, 32, 18, metal_dark);
    draw_rect(pixels, w, h, ox + 12, oy - 60, 28, 14, metal_mid);
    /* Top highlight (light catching the flat top) */
    draw_rect(pixels, w, h, ox + 12, oy - 62, 28,  2, metal_hi);
    /* Ejection port */
    draw_rect(pixels, w, h, ox + 24, oy - 56, 10,  6, metal_dark);
    draw_rect(pixels, w, h, ox + 25, oy - 55,  8,  4, 0xFF353535);
    /* Slide serrations (rear) */
    for (int i = 0; i < 4; i++) {
        draw_rect(pixels, w, h, ox + 32 + i * 2, oy - 58, 1, 10, metal_light);
    }

    /* --- Barrel (extends forward from slide) --- */
    draw_rect(pixels, w, h, ox + 4, oy - 58, 8, 12, metal_dark);
    draw_rect(pixels, w, h, ox + 5, oy - 57, 6, 10, metal_mid);
    /* Muzzle opening */
    draw_rect(pixels, w, h, ox + 6, oy - 55, 4,  4, 0xFF1A1A1A);

    /* --- Front sight --- */
    draw_rect(pixels, w, h, ox + 7, oy - 64, 3, 3, sight_col);
    draw_pixel(pixels, w, h, ox + 8, oy - 65, 0xFFFFFFFF); /* sight dot */

    /* --- Rear sight --- */
    draw_rect(pixels, w, h, ox + 34, oy - 65, 3, 3, sight_col);
    draw_rect(pixels, w, h, ox + 39, oy - 65, 3, 3, sight_col);

    /* --- Hammer (visible at rear) --- */
    draw_rect(pixels, w, h, ox + 40, oy - 56, 3,  8, metal_dark);
    draw_rect(pixels, w, h, ox + 39, oy - 58, 5,  3, metal_mid);

    /* --- Muzzle flash --- */
    if (wpn->fired_this_frame) {
        uint32_t flash_bright = 0xFFFFEE66;
        uint32_t flash_mid    = 0xFFFF9922;
        uint32_t flash_outer  = 0xFFCC6600;
        int fx = ox + 7;
        int fy = oy - 62;

        /* Outer glow */
        for (int dy = -10; dy <= 10; dy++) {
            for (int dx = -8; dx <= 8; dx++) {
                int d2 = dx * dx + dy * dy;
                if (d2 <= 100) {
                    int px = fx + dx, py = fy + dy;
                    uint32_t c = d2 < 20 ? flash_bright :
                                 d2 < 50 ? flash_mid : flash_outer;
                    draw_pixel(pixels, w, h, px, py, c);
                }
            }
        }
        /* Directional streaks upward */
        for (int i = 0; i < 6; i++) {
            draw_pixel(pixels, w, h, fx - 1, fy - 10 - i, flash_mid);
            draw_pixel(pixels, w, h, fx,     fy - 10 - i, flash_bright);
            draw_pixel(pixels, w, h, fx + 1, fy - 10 - i, flash_mid);
        }
    }
}
