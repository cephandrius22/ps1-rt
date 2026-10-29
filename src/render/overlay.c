#include "overlay.h"
#include <stdio.h>
#include "font.h"
#include "ps1_effects.h"
#include <stddef.h>
#include <math.h>

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

/* Letters A-Z, same encoding (e.g. 'A' = 010 101 111 101 101) */
static const unsigned char letter_rows[26][GLYPH_H] = {
    {2, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {3, 4, 4, 4, 3}, {6, 5, 5, 5, 6}, /* A-D */
    {7, 4, 6, 4, 7}, {7, 4, 6, 4, 4}, {3, 4, 5, 5, 3}, {5, 5, 7, 5, 5}, /* E-H */
    {7, 2, 2, 2, 7}, {1, 1, 1, 5, 2}, {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7}, /* I-L */
    {5, 7, 7, 5, 5}, {6, 5, 5, 5, 5}, {2, 5, 5, 5, 2}, {6, 5, 6, 4, 4}, /* M-P */
    {2, 5, 5, 6, 3}, {6, 5, 6, 5, 5}, {3, 4, 2, 1, 6}, {7, 2, 2, 2, 2}, /* Q-T */
    {5, 5, 5, 5, 7}, {5, 5, 5, 5, 2}, {5, 5, 7, 7, 5}, {5, 5, 2, 5, 5}, /* U-X */
    {5, 5, 2, 2, 2}, {7, 1, 2, 4, 7},                                   /* Y-Z */
};

/* Icons and punctuation, same encoding */
static const unsigned char cross_rows[GLYPH_H] = {0, 2, 7, 2, 0};
static const unsigned char slash_rows[GLYPH_H] = {1, 1, 2, 4, 4};
static const unsigned char dot_rows[GLYPH_H]   = {0, 0, 0, 0, 2};
static const unsigned char dash_rows[GLYPH_H]  = {0, 0, 7, 0, 0};
static const unsigned char colon_rows[GLYPH_H] = {0, 2, 0, 2, 0};

#define HUD_MARGIN      6
#define HUD_SHADOW      0xFF000000u
#define HUD_HEALTH      0xFFC83C32u /* dull red */
#define HUD_AMMO        0xFFC8B464u /* brass */
#define HUD_DIGITS_MAX  3
#define HUD_TITLE       0xFFD8D0B8u /* bone white */
#define DEAD_TINT_KEEP  0.35f       /* fraction of green/blue kept when dead */
#define FLASH_MAX_ALPHA 0.55f       /* strongest flash still shows the scene */
#define END_DIM         0.3f        /* brightness kept behind the level-end text */
#define DEBUG_SCALE     1           /* debug text: 3x5 pixels per glyph */
#define VIEW_SCALE      2           /* viewmodel texel → 2x2 screen pixels */
#define DEBUG_MARGIN    3
#define DEBUG_LINE_GAP  2           /* pixels between debug lines */
#define MESSAGE_Y       28          /* top of the notice line */
#define MESSAGE_COLOR   0xFFD8D0B8u /* bone white */
#define BATTERY_W       30          /* HUD battery bar, pixels */
#define BATTERY_H       3
#define BATTERY_GAP     3           /* above the health number */
#define BATTERY_LOW     0.2f        /* bar turns red below this */
#define BATTERY_COLOR   0xFFD8D0B8u /* bone white */
#define BATTERY_EMPTY   0xFF302820u
#define KEYCARD_W       5           /* HUD keycard icon, pixels */
#define KEYCARD_H       8
#define KEYCARD_GAP     3
#define DEBUG_OK_COLOR   0xFF8CD278u /* green */
#define DEBUG_WARN_COLOR 0xFFE6C850u /* yellow */
#define DEBUG_OVER_COLOR 0xFFE65046u /* red */

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
        int shadow = scale / 2 > 0 ? scale / 2 : 1;
        int off = pass == 0 ? shadow : 0;
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

/* Glyph for a character, or NULL for space/unsupported (drawn blank) */
static const unsigned char *glyph_for(char c) {
    if (c >= '0' && c <= '9') {
        return digit_rows[c - '0'];
    }
    if (c >= 'A' && c <= 'Z') {
        return letter_rows[c - 'A'];
    }
    if (c >= 'a' && c <= 'z') {
        return letter_rows[c - 'a'];
    }
    switch (c) {
    case '.': return dot_rows;
    case '-': return dash_rows;
    case '/': return slash_rows;
    case ':': return colon_rows;
    default:  return NULL;
    }
}

static void draw_text(Framebuffer *fb, const char *text, int x, int y, int scale, uint32_t color) {
    for (const char *c = text; *c; c++) {
        const unsigned char *rows = glyph_for(*c);
        if (rows) {
            draw_glyph(fb, rows, x, y, scale, color);
        }
        x += glyph_advance(scale);
    }
}

static uint32_t severity_color(DebugSeverity severity) {
    switch (severity) {
    case DEBUG_OK:   return DEBUG_OK_COLOR;
    case DEBUG_WARN: return DEBUG_WARN_COLOR;
    case DEBUG_OVER: return DEBUG_OVER_COLOR;
    }
    return DEBUG_OK_COLOR;
}

static void draw_debug(Framebuffer *fb, const OverlayState *state) {
    int y = DEBUG_MARGIN;
    for (int i = 0; i < state->debug_count && i < OVERLAY_DEBUG_LINES; i++) {
        const DebugLine *line = &state->debug[i];
        draw_text(fb, line->text, DEBUG_MARGIN, y, DEBUG_SCALE, severity_color(line->severity));
        y += GLYPH_H * DEBUG_SCALE + DEBUG_LINE_GAP;
    }
}

/* Blend every pixel toward its own grey by amount (low health) */
static void desaturate(Framebuffer *fb, float amount) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        uint32_t p = fb->pixels[i];
        float r = (float)((p >> 16) & 0xFF);
        float g = (float)((p >> 8) & 0xFF);
        float b = (float)(p & 0xFF);
        float grey = 0.299f * r + 0.587f * g + 0.114f * b; /* Rec. 601 luma */
        uint32_t nr = (uint32_t)(r + (grey - r) * amount);
        uint32_t ng = (uint32_t)(g + (grey - g) * amount);
        uint32_t nb = (uint32_t)(b + (grey - b) * amount);
        fb->pixels[i] = 0xFF000000u | (nr << 16) | (ng << 8) | nb;
    }
}

/* Blend every pixel toward color by alpha (damage flash) */
static void tint_flash(Framebuffer *fb, Vec3 color, float alpha) {
    float cr = color.x * 255.0f;
    float cg = color.y * 255.0f;
    float cb = color.z * 255.0f;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        uint32_t p = fb->pixels[i];
        float r = (float)((p >> 16) & 0xFF);
        float g = (float)((p >> 8) & 0xFF);
        float b = (float)(p & 0xFF);
        uint32_t nr = (uint32_t)(r + (cr - r) * alpha);
        uint32_t ng = (uint32_t)(g + (cg - g) * alpha);
        uint32_t nb = (uint32_t)(b + (cb - b) * alpha);
        fb->pixels[i] = 0xFF000000u | (nr << 16) | (ng << 8) | nb;
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

#define STATS_SCALE   2   /* intermission lines: 5x7 font at 2x */
#define STATS_LINE    (FONT_ROWS * STATS_SCALE + 6)
#define STATS_LINES   6   /* title, blank, kills, secrets, time, next */
#define STATS_TAB     78  /* pixels from the label to its value */
#define SECONDS_PER_MINUTE 60

static void draw_centered(Framebuffer *fb, const char *text, int y, int scale) {
    font_draw(fb, text, (SCREEN_W - font_width(text, scale)) / 2, y, scale, HUD_TITLE);
}

/* "LABEL  value", the label left of center and the values lined up */
static void draw_stat(Framebuffer *fb, const char *label, const char *value, int y) {
    int x = SCREEN_W / 2 - STATS_TAB;
    font_draw(fb, label, x, y, STATS_SCALE, HUD_TITLE);
    font_draw(fb, value, x + STATS_TAB + STATS_TAB / 2, y, STATS_SCALE, HUD_TITLE);
}

/* Doom-style tally between maps:
 *
 *        PROCESSING PLANT
 *
 *     KILLS        3/4
 *     SECRETS      1/1
 *     TIME        2:31
 *          NEXT: ARENA
 */
static void draw_intermission(Framebuffer *fb, const OverlayState *s) {
    char value[24];
    int y = (SCREEN_H - STATS_LINES * STATS_LINE) / 2;
    if (s->map_title && s->map_title[0]) {
        draw_centered(fb, s->map_title, y, STATS_SCALE);
    }
    y += 2 * STATS_LINE;

    snprintf(value, sizeof(value), "%d/%d", s->kills, s->kills_total);
    draw_stat(fb, "KILLS", value, y);
    y += STATS_LINE;
    snprintf(value, sizeof(value), "%d/%d", s->secrets, s->secrets_total);
    draw_stat(fb, "SECRETS", value, y);
    y += STATS_LINE;
    snprintf(value, sizeof(value), "%d:%02d", s->time_seconds / SECONDS_PER_MINUTE,
             s->time_seconds % SECONDS_PER_MINUTE);
    draw_stat(fb, "TIME", value, y);
    y += STATS_LINE;

    if (s->next_title && s->next_title[0]) {
        y += STATS_LINE / 2;
        snprintf(value, sizeof(value), "NEXT: %s", s->next_title);
        draw_centered(fb, value, y, 1);
    }
}

/* Centered notice with a 1 px drop shadow so it reads on bright walls */
static void draw_message(Framebuffer *fb, const char *text) {
    int x = (SCREEN_W - font_width(text, 1)) / 2;
    font_draw(fb, text, x + 1, MESSAGE_Y + 1, 1, HUD_SHADOW);
    font_draw(fb, text, x, MESSAGE_Y, 1, MESSAGE_COLOR);
}

static uint32_t to_pixel(Vec3 c) {
    uint32_t r = (uint32_t)(fminf(fmaxf(c.x, 0), 1) * 255.0f);
    uint32_t g = (uint32_t)(fminf(fmaxf(c.y, 0), 1) * 255.0f);
    uint32_t b = (uint32_t)(fminf(fmaxf(c.z, 0), 1) * 255.0f);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

/* Thin bar above the health number */
static void draw_battery(Framebuffer *fb, float battery, int above_y) {
    int y = above_y - BATTERY_GAP - BATTERY_H;
    int filled = (int)(battery * BATTERY_W + 0.5f);
    uint32_t color = battery < BATTERY_LOW ? HUD_HEALTH : BATTERY_COLOR;
    for (int dy = -1; dy <= BATTERY_H; dy++) {
        for (int dx = -1; dx <= BATTERY_W; dx++) {
            bool edge = dx < 0 || dy < 0 || dx == BATTERY_W || dy == BATTERY_H;
            uint32_t c = edge ? HUD_SHADOW : (dx < filled ? color : BATTERY_EMPTY);
            put_pixel(fb, HUD_MARGIN + dx, y + dy, c);
        }
    }
}

/* Small upright cards in a row, right-aligned above the ammo count */
static void draw_keycards(Framebuffer *fb, const OverlayState *state, int above_y) {
    int y = above_y - KEYCARD_GAP - KEYCARD_H;
    int x = SCREEN_W - HUD_MARGIN - KEYCARD_W;
    for (int i = state->keycard_count - 1; i >= 0; i--) {
        uint32_t color = to_pixel(state->keycard_colors[i]);
        for (int dy = -1; dy <= KEYCARD_H; dy++) {
            for (int dx = -1; dx <= KEYCARD_W; dx++) {
                bool edge = dx < 0 || dy < 0 || dx == KEYCARD_W || dy == KEYCARD_H;
                put_pixel(fb, x + dx, y + dy, edge ? HUD_SHADOW : color);
            }
        }
        x -= KEYCARD_W + KEYCARD_GAP;
    }
}

#define EPISODE_END_TEXT  "EPISODE COMPLETE"
#define EPISODE_END_SCALE 2

/* Big headline in the middle, the episode's title under it */
static void draw_episode_end(Framebuffer *fb, const char *title) {
    int y = SCREEN_H / 2 - FONT_ROWS * EPISODE_END_SCALE;
    int x = (SCREEN_W - font_width(EPISODE_END_TEXT, EPISODE_END_SCALE)) / 2;
    font_draw(fb, EPISODE_END_TEXT, x, y, EPISODE_END_SCALE, HUD_TITLE);
    if (!title || !title[0]) {
        return;
    }
    y += FONT_ROWS * EPISODE_END_SCALE + FONT_ROWS;
    font_draw(fb, title, (SCREEN_W - font_width(title, 1)) / 2, y, 1, HUD_TITLE);
}

void overlay_init(Overlay *o) {
    for (int i = 0; i < PTEX_COUNT; i++) {
        o->generated[i] = false;
        o->views[i].pixels = NULL;
    }
}

void overlay_free(Overlay *o) {
    for (int i = 0; i < PTEX_COUNT; i++) {
        if (o->generated[i]) {
            texture_free(&o->views[i]);
        }
        o->generated[i] = false;
    }
}

static const Texture *view_texture(Overlay *o, const char *name) {
    int id = name ? proctex_find(name) : -1;
    if (id < 0) {
        return NULL;
    }
    if (!o->generated[id]) {
        proctex_generate(&o->views[id], (ProceduralTextureID)id);
        o->generated[id] = true;
    }
    return &o->views[id];
}

/* Lit and dithered like the world, so the gun doesn't look pasted on */
static uint32_t lit_pixel(const uint8_t *rgb, float light, int x, int y) {
    uint8_t r = (uint8_t)fminf(rgb[0] * light, 255.0f);
    uint8_t g = (uint8_t)fminf(rgb[1] * light, 255.0f);
    uint8_t b = (uint8_t)fminf(rgb[2] * light, 255.0f);
    return ps1_dither_color(r, g, b, x, y);
}

/* Viewmodel: one sheet frame, scaled VIEW_SCALE, bottom-center, offset by
 * bob/recoil/switch. Nearest-neighbour; color-key texels are skipped. */
static void draw_viewmodel(Overlay *o, Framebuffer *fb, const ViewmodelState *vm) {
    const Texture *tex = view_texture(o, vm->texture);
    if (!tex) {
        return;
    }

    int w = VIEW_FRAME_W * VIEW_SCALE;
    int h = VIEW_FRAME_H * VIEW_SCALE;
    int x0 = (SCREEN_W - w) / 2 + (int)vm->offset_x;
    int y0 = SCREEN_H - h + (int)vm->offset_y;
    int col0 = vm->frame * VIEW_FRAME_W;

    for (int y = 0; y < h; y++) {
        int sy = y0 + y;
        if (sy < 0 || sy >= SCREEN_H) {
            continue;
        }
        for (int x = 0; x < w; x++) {
            int sx = x0 + x;
            if (sx < 0 || sx >= SCREEN_W) {
                continue;
            }
            const uint8_t *p = tex->pixels + ((y / VIEW_SCALE) * tex->width +
                                              col0 + x / VIEW_SCALE) * 3;
            bool key = p[0] == TEXTURE_KEY_R && p[1] == TEXTURE_KEY_G && p[2] == TEXTURE_KEY_B;
            if (!key) {
                fb->pixels[sy * SCREEN_W + sx] = lit_pixel(p, vm->light, sx, sy);
            }
        }
    }
}

void overlay_draw(Overlay *o, Framebuffer *fb, const OverlayState *state) {
    draw_viewmodel(o, fb, &state->viewmodel);
    if (state->desaturate > 0) {
        desaturate(fb, fminf(state->desaturate, 1.0f));
    }
    if (state->flash > 0) {
        tint_flash(fb, state->flash_color, state->flash * FLASH_MAX_ALPHA);
    }

    switch (state->mode) {
    case OVERLAY_PLAYING:
        break;
    case OVERLAY_DEAD:
        tint_dead(fb);
        break;
    case OVERLAY_LEVEL_END:
        dim(fb, END_DIM);
        draw_intermission(fb, state);
        break;
    case OVERLAY_EPISODE_END:
        dim(fb, END_DIM);
        draw_episode_end(fb, state->title);
        break;
    }

    if (state->message) {
        draw_message(fb, state->message);
    }

    int y = SCREEN_H - HUD_MARGIN - GLYPH_H * HUD_SCALE;

    /* Health: cross icon + number, bottom left */
    int x = HUD_MARGIN;
    draw_glyph(fb, cross_rows, x, y, HUD_SCALE, HUD_HEALTH);
    draw_number(fb, state->health, x + glyph_advance(HUD_SCALE), y, HUD_SCALE, HUD_HEALTH);

    /* Ammo: number, bottom right */
    int ammo_x = SCREEN_W - HUD_MARGIN - number_width(state->ammo, HUD_SCALE);
    draw_number(fb, state->ammo, ammo_x, y, HUD_SCALE, HUD_AMMO);
    draw_keycards(fb, state, y);
    if (state->show_battery) {
        draw_battery(fb, state->battery, y);
    }

    draw_debug(fb, state);
}
