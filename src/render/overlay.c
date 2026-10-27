#include "overlay.h"

/* 3x5 pixel digits, one row per 3-bit value (MSB = left pixel).
 * Example: '1' = 010 110 010 010 111 */
#define GLYPH_W     3
#define GLYPH_H     5
#define GLYPH_SCALE 2   /* each font pixel drawn as 2x2 */
#define GLYPH_GAP   1   /* font pixels between digits */

static const unsigned char digit_rows[10][GLYPH_H] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
    {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1},
    {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};

/* Health cross icon, same encoding */
static const unsigned char cross_rows[GLYPH_H] = {0, 2, 7, 2, 0};

#define HUD_MARGIN      6
#define HUD_SHADOW      0xFF000000u
#define HUD_HEALTH      0xFFC83C32u /* dull red */
#define HUD_AMMO        0xFFC8B464u /* brass */
#define HUD_DIGITS_MAX  3
#define DEAD_TINT_KEEP  0.35f       /* fraction of green/blue kept when dead */

static void put_pixel(Framebuffer *fb, int x, int y, uint32_t color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) {
        return;
    }
    fb->pixels[y * SCREEN_W + x] = color;
}

/* One glyph at (x, y) top-left, with a 1px drop shadow for legibility */
static void draw_glyph(Framebuffer *fb, const unsigned char rows[GLYPH_H],
                       int x, int y, uint32_t color) {
    for (int pass = 0; pass < 2; pass++) {
        int off = pass == 0 ? 1 : 0;
        uint32_t c = pass == 0 ? HUD_SHADOW : color;

        for (int row = 0; row < GLYPH_H; row++) {
            for (int col = 0; col < GLYPH_W; col++) {
                if (!(rows[row] & (1 << (GLYPH_W - 1 - col)))) {
                    continue;
                }
                for (int sy = 0; sy < GLYPH_SCALE; sy++) {
                    for (int sx = 0; sx < GLYPH_SCALE; sx++) {
                        put_pixel(fb, x + col * GLYPH_SCALE + sx + off,
                                  y + row * GLYPH_SCALE + sy + off, c);
                    }
                }
            }
        }
    }
}

static int glyph_advance(void) {
    return (GLYPH_W + GLYPH_GAP) * GLYPH_SCALE;
}

/* Draw a non-negative number (up to HUD_DIGITS_MAX digits) from x */
static void draw_number(Framebuffer *fb, int value, int x, int y, uint32_t color) {
    char digits[HUD_DIGITS_MAX];
    int n = 0;
    value = value < 0 ? 0 : value;
    do {
        digits[n++] = (char)(value % 10);
        value /= 10;
    } while (value > 0 && n < HUD_DIGITS_MAX);

    for (int i = n - 1; i >= 0; i--) {
        draw_glyph(fb, digit_rows[(int)digits[i]], x, y, color);
        x += glyph_advance();
    }
}

static int number_width(int value) {
    int n = 1;
    while (value >= 10 && n < HUD_DIGITS_MAX) {
        value /= 10;
        n++;
    }
    return n * glyph_advance();
}

/* Keep red, crush green/blue: the world turns blood-coloured */
static void tint_dead(Framebuffer *fb) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        uint32_t p = fb->pixels[i];
        uint32_t g = (uint32_t)(((p >> 8) & 0xFF) * DEAD_TINT_KEEP);
        uint32_t b = (uint32_t)((p & 0xFF) * DEAD_TINT_KEEP);
        fb->pixels[i] = (p & 0xFFFF0000u) | (g << 8) | b;
    }
}

void overlay_draw(Framebuffer *fb, const OverlayState *state) {
    if (state->mode == OVERLAY_DEAD) {
        tint_dead(fb);
    }

    int y = SCREEN_H - HUD_MARGIN - GLYPH_H * GLYPH_SCALE;

    /* Health: cross icon + number, bottom left */
    int x = HUD_MARGIN;
    draw_glyph(fb, cross_rows, x, y, HUD_HEALTH);
    draw_number(fb, state->health, x + glyph_advance(), y, HUD_HEALTH);

    /* Ammo: number, bottom right */
    int ammo_x = SCREEN_W - HUD_MARGIN - number_width(state->ammo);
    draw_number(fb, state->ammo, ammo_x, y, HUD_AMMO);
}
