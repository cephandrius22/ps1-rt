#include "overlay.h"

/* 3x5 pixel digits, one row per 3-bit value (MSB = left pixel).
 * Example: '1' = 010 110 010 010 111 */
#define GLYPH_W     3
#define GLYPH_H     5
#define HUD_SCALE   2   /* HUD font pixel drawn as 2x2 */
#define TITLE_SCALE 5   /* level-end numbers */
#define GLYPH_GAP   1   /* font pixels between digits */

static const unsigned char digit_rows[10][GLYPH_H] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
    {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1},
    {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};

/* Icons, same encoding */
static const unsigned char cross_rows[GLYPH_H] = {0, 2, 7, 2, 0};
static const unsigned char slash_rows[GLYPH_H] = {1, 1, 2, 4, 4};

#define HUD_MARGIN      6
#define HUD_SHADOW      0xFF000000u
#define HUD_HEALTH      0xFFC83C32u /* dull red */
#define HUD_AMMO        0xFFC8B464u /* brass */
#define HUD_DIGITS_MAX  3
#define HUD_TITLE       0xFFD8D0B8u /* bone white */
#define DEAD_TINT_KEEP  0.35f       /* fraction of green/blue kept when dead */
#define END_DIM         0.3f        /* brightness kept behind the level-end text */

static void put_pixel(Framebuffer *fb, int x, int y, uint32_t color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) {
        return;
    }
    fb->pixels[y * SCREEN_W + x] = color;
}

/* One glyph at (x, y) top-left, each font pixel scale x scale, with a
 * drop shadow for legibility */
static void draw_glyph(Framebuffer *fb, const unsigned char rows[GLYPH_H],
                       int x, int y, int scale, uint32_t color) {
    for (int pass = 0; pass < 2; pass++) {
        int off = pass == 0 ? scale / 2 : 0;
        uint32_t c = pass == 0 ? HUD_SHADOW : color;

        for (int row = 0; row < GLYPH_H; row++) {
            for (int col = 0; col < GLYPH_W; col++) {
                if (!(rows[row] & (1 << (GLYPH_W - 1 - col)))) {
                    continue;
                }
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        put_pixel(fb, x + col * scale + sx + off,
                                  y + row * scale + sy + off, c);
                    }
                }
            }
        }
    }
}

static int glyph_advance(int scale) {
    return (GLYPH_W + GLYPH_GAP) * scale;
}

/* Draw a non-negative number (up to HUD_DIGITS_MAX digits) from x.
 * Returns the x just past the last digit. */
static int draw_number(Framebuffer *fb, int value, int x, int y, int scale, uint32_t color) {
    char digits[HUD_DIGITS_MAX];
    int n = 0;
    value = value < 0 ? 0 : value;
    do {
        digits[n++] = (char)(value % 10);
        value /= 10;
    } while (value > 0 && n < HUD_DIGITS_MAX);

    for (int i = n - 1; i >= 0; i--) {
        draw_glyph(fb, digit_rows[(int)digits[i]], x, y, scale, color);
        x += glyph_advance(scale);
    }
    return x;
}

static int number_width(int value, int scale) {
    int n = 1;
    while (value >= 10 && n < HUD_DIGITS_MAX) {
        value /= 10;
        n++;
    }
    return n * glyph_advance(scale);
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

static void dim(Framebuffer *fb, float keep) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        uint32_t p = fb->pixels[i];
        uint32_t r = (uint32_t)(((p >> 16) & 0xFF) * keep);
        uint32_t g = (uint32_t)(((p >> 8) & 0xFF) * keep);
        uint32_t b = (uint32_t)((p & 0xFF) * keep);
        fb->pixels[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}

/* "kills/total" centered on screen */
static void draw_kill_count(Framebuffer *fb, int kills, int total) {
    int width = number_width(kills, TITLE_SCALE) + glyph_advance(TITLE_SCALE)
              + number_width(total, TITLE_SCALE);
    int x = (SCREEN_W - width) / 2;
    int y = (SCREEN_H - GLYPH_H * TITLE_SCALE) / 2;

    x = draw_number(fb, kills, x, y, TITLE_SCALE, HUD_TITLE);
    draw_glyph(fb, slash_rows, x, y, TITLE_SCALE, HUD_TITLE);
    draw_number(fb, total, x + glyph_advance(TITLE_SCALE), y, TITLE_SCALE, HUD_TITLE);
}

void overlay_draw(Framebuffer *fb, const OverlayState *state) {
    switch (state->mode) {
    case OVERLAY_PLAYING:
        break;
    case OVERLAY_DEAD:
        tint_dead(fb);
        break;
    case OVERLAY_LEVEL_END:
        dim(fb, END_DIM);
        draw_kill_count(fb, state->kills, state->kills_total);
        break;
    }

    int y = SCREEN_H - HUD_MARGIN - GLYPH_H * HUD_SCALE;

    /* Health: cross icon + number, bottom left */
    int x = HUD_MARGIN;
    draw_glyph(fb, cross_rows, x, y, HUD_SCALE, HUD_HEALTH);
    draw_number(fb, state->health, x + glyph_advance(HUD_SCALE), y, HUD_SCALE, HUD_HEALTH);

    /* Ammo: number, bottom right */
    int ammo_x = SCREEN_W - HUD_MARGIN - number_width(state->ammo, HUD_SCALE);
    draw_number(fb, state->ammo, ammo_x, y, HUD_SCALE, HUD_AMMO);
}
