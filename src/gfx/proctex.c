#include "proctex.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* --- Hash-based noise --- */

static uint32_t hash2d(int x, int y) {
    uint32_t h = (uint32_t)(x * 374761393 + y * 668265263);
    h = (h ^ (h >> 13)) * 1274126177;
    return h ^ (h >> 16);
}

static float noise01(int x, int y) {
    return (float)(hash2d(x, y) & 0xFFFF) / 65535.0f;
}

/* --- Texture allocation helper --- */

static void tex_alloc(Texture *tex, int w, int h) {
    tex->width = w;
    tex->height = h;
    tex->pixels = malloc((size_t)(w * h * 3));
}

static void tex_set(Texture *tex, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    int i = (y * tex->width + x) * 3;
    tex->pixels[i] = r;
    tex->pixels[i + 1] = g;
    tex->pixels[i + 2] = b;
}

static uint8_t clamp8(float v) {
    if (v < 0) {
        return 0;
    }
    if (v > 255) {
        return 255;
    }
    return (uint8_t)v;
}

/* --- Texture generators --- */

static void gen_concrete(Texture *tex) {
    tex_alloc(tex, 32, 32);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            float base = 0.28f + noise01(x, y) * 0.14f - 0.07f;
            /* Darker stain patches */
            float stain = noise01(x / 4, y / 4) * 0.12f;
            /* Water damage streaks */
            float streak = noise01(x, y / 5) * 0.06f;
            float v = base - stain - streak;
            /* Grimy yellow-brown tint */
            tex_set(tex, x, y, clamp8(v * 270.0f), clamp8(v * 255.0f), clamp8(v * 210.0f));
        }
    }
}

static void gen_brick(Texture *tex) {
    tex_alloc(tex, 64, 64);
    int brick_h = 8;  /* brick height in pixels */
    int brick_w = 16; /* brick width in pixels */
    int mortar = 1;

    for (int y = 0; y < 64; y++) {
        int row = y / brick_h;
        int y_in_brick = y % brick_h;
        int offset = (row % 2) * (brick_w / 2);

        for (int x = 0; x < 64; x++) {
            int bx = (x + offset) % 64;
            int x_in_brick = bx % brick_w;

            /* Mortar lines — dark, crumbling */
            if (y_in_brick < mortar || x_in_brick < mortar) {
                float mv = 0.18f + noise01(x + 200, y + 200) * 0.08f;
                tex_set(tex, x, y, clamp8(mv * 260), clamp8(mv * 240), clamp8(mv * 200));
                continue;
            }

            /* Brick face — darker, dirtier, more variation */
            int brick_id_x = bx / brick_w;
            int brick_id_y = row;
            float variation = noise01(brick_id_x * 7 + 31, brick_id_y * 13 + 47) * 0.20f - 0.10f;
            float detail = noise01(x * 3 + 100, y * 3 + 100) * 0.06f - 0.03f;
            float grime = noise01(x + 500, y / 2 + 500) * 0.08f;

            float r = 0.38f + variation + detail - grime;
            float g = 0.22f + variation * 0.5f + detail - grime;
            float b = 0.15f + variation * 0.3f + detail - grime;
            tex_set(tex, x, y, clamp8(r * 255), clamp8(g * 255), clamp8(b * 255));
        }
    }
}

static void gen_metal_panel(Texture *tex) {
    tex_alloc(tex, 32, 32);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            float base = 0.22f + noise01(x, y) * 0.06f;

            /* Bevel edges */
            int dx = (x < 16) ? x : 31 - x;
            int dy = (y < 16) ? y : 31 - y;
            int edge = (dx < dy) ? dx : dy;

            if (edge == 0) {
                base -= 0.06f;
            } else if (edge == 1) {
                base += 0.04f;
            }

            /* Corner rivets */
            int corners[4][2] = {{4, 4}, {27, 4}, {4, 27}, {27, 27}};
            for (int c = 0; c < 4; c++) {
                int rx = x - corners[c][0];
                int ry = y - corners[c][1];
                if (rx * rx + ry * ry <= 2) {
                    base += 0.07f;
                }
            }

            /* Rust bleed from rivets */
            for (int c = 0; c < 4; c++) {
                int dy2 = y - corners[c][1];
                int dx2 = x - corners[c][0];
                if (dy2 > 0 && dy2 < 8 && dx2 > -2 && dx2 < 3) {
                    float rust = 0.04f * (1.0f - (float)dy2 / 8.0f);
                    base -= rust * 0.5f;
                    tex_set(tex, x, y,
                        clamp8((base + rust) * 255),
                        clamp8(base * 240),
                        clamp8((base - rust) * 220));
                    goto next_metal;
                }
            }

            tex_set(tex, x, y, clamp8(base * 250), clamp8(base * 250), clamp8(base * 260));
            next_metal:;
        }
    }
}

static void gen_metal_grate(Texture *tex) {
    tex_alloc(tex, 32, 32);
    int bar_spacing = 4;
    int bar_width = 1;

    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            int on_h_bar = (y % bar_spacing) < bar_width;
            int on_v_bar = (x % bar_spacing) < bar_width;

            if (on_h_bar || on_v_bar) {
                float v = 0.30f + noise01(x, y) * 0.06f;
                /* Rust spots on bars */
                if (noise01(x * 3 + 70, y * 3 + 70) > 0.7f) {
                    tex_set(tex, x, y, clamp8(v * 300), clamp8(v * 230), clamp8(v * 180));
                } else {
                    tex_set(tex, x, y, clamp8(v * 250), clamp8(v * 250), clamp8(v * 255));
                }
            } else {
                float v = 0.04f + noise01(x + 50, y + 50) * 0.03f;
                tex_set(tex, x, y, clamp8(v * 255), clamp8(v * 255), clamp8(v * 255));
            }
        }
    }
}

static void gen_rust(Texture *tex) {
    tex_alloc(tex, 32, 32);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            float n1 = noise01(x, y / 3);
            float n2 = noise01(x * 2 + 77, y + 33);
            float n3 = noise01(x / 2 + 40, y + 60);
            float blend = n1 * 0.5f + n2 * 0.3f + n3 * 0.2f;

            float r = 0.40f + blend * 0.25f;
            float g = 0.18f + blend * 0.10f;
            float b = 0.06f + blend * 0.04f;

            /* Heavy pitting and corrosion */
            if (noise01(x * 5, y * 5) > 0.75f) {
                r -= 0.12f;
                g -= 0.08f;
                b -= 0.04f;
            }
            /* Flaking — very dark spots */
            if (noise01(x * 7 + 13, y * 7 + 17) > 0.92f) {
                r = 0.10f; g = 0.06f; b = 0.03f;
            }

            tex_set(tex, x, y, clamp8(r * 255), clamp8(g * 255), clamp8(b * 255));
        }
    }
}

static void gen_floor_tile(Texture *tex) {
    tex_alloc(tex, 32, 32);
    int tile_size = 16;
    int grout = 1;

    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            int tx = x % tile_size;
            int ty = y % tile_size;

            /* Grout lines — dark, dirty */
            if (tx < grout || ty < grout) {
                float v = 0.12f + noise01(x + 300, y + 300) * 0.04f;
                tex_set(tex, x, y, clamp8(v * 260), clamp8(v * 245), clamp8(v * 220));
                continue;
            }

            /* Stained, cracked tiles */
            int tile_col = x / tile_size;
            int tile_row = y / tile_size;
            float base = ((tile_col + tile_row) % 2 == 0) ? 0.25f : 0.20f;
            base += noise01(x, y) * 0.05f;
            /* Dirt accumulation */
            base -= noise01(x / 3 + 400, y / 3 + 400) * 0.06f;

            tex_set(tex, x, y, clamp8(base * 265), clamp8(base * 250), clamp8(base * 220));
        }
    }
}

static void gen_pipe(Texture *tex) {
    tex_alloc(tex, 32, 32);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            float cx = (float)x / 31.0f;
            float highlight = 1.0f - fabsf(cx - 0.5f) * 2.0f;
            highlight = highlight * highlight;

            float base = 0.18f + highlight * 0.18f;
            base += noise01(x, y) * 0.04f;

            /* Band/rivet lines every 8 pixels */
            if (y % 8 == 0) {
                base += 0.06f;
            }

            /* Corroded, brownish */
            float rust_spot = noise01(x * 3 + 90, y * 3 + 90);
            if (rust_spot > 0.8f) {
                tex_set(tex, x, y, clamp8(base * 300), clamp8(base * 220), clamp8(base * 160));
            } else {
                tex_set(tex, x, y, clamp8(base * 250), clamp8(base * 245), clamp8(base * 240));
            }
        }
    }
}

/* --- Sprites (color-keyed) --- */

static void tex_set_key(Texture *tex, int x, int y) {
    tex_set(tex, x, y, TEXTURE_KEY_R, TEXTURE_KEY_G, TEXTURE_KEY_B);
}

#define BARREL_W      32
#define BARREL_H      40
#define BARREL_LEFT   5   /* body spans [LEFT, W - LEFT) */
#define BARREL_TOP    3   /* rows above are empty */
#define BARREL_OOZE   6   /* glowing sludge rows below the top */
#define BARREL_BAND_1 14
#define BARREL_BAND_2 28

/* Rusted drum of glowing sludge. Rows run top (y=0) to bottom.
 *
 *     ~~~~~~~~      <- green ooze
 *    |  rust  |
 *    |========|     <- bands
 *    |        |
 */
static void gen_barrel(Texture *tex) {
    tex_alloc(tex, BARREL_W, BARREL_H);
    float half = (BARREL_W - 2 * BARREL_LEFT) * 0.5f;
    float center = BARREL_W * 0.5f;

    for (int y = 0; y < BARREL_H; y++) {
        for (int x = 0; x < BARREL_W; x++) {
            bool inside = x >= BARREL_LEFT && x < BARREL_W - BARREL_LEFT && y >= BARREL_TOP;
            if (!inside) {
                tex_set_key(tex, x, y);
                continue;
            }

            /* Cylinder shading: bright down the middle, dark at the edges */
            float across = (x + 0.5f - center) / half;
            float shade = 0.35f + 0.65f * sqrtf(fmaxf(0.0f, 1.0f - across * across));
            float n = noise01(x * 5 + 11, y * 5 + 13);

            if (y < BARREL_TOP + BARREL_OOZE) {
                float glow = 0.6f + 0.4f * n;
                tex_set(tex, x, y, clamp8(40 * glow), clamp8(230 * glow), clamp8(60 * glow));
                continue;
            }

            bool band = y == BARREL_BAND_1 || y == BARREL_BAND_1 + 1 ||
                        y == BARREL_BAND_2 || y == BARREL_BAND_2 + 1;
            float base = band ? 0.30f : 0.45f + 0.2f * n;
            float v = base * shade * 255.0f;
            tex_set(tex, x, y, clamp8(v * 1.2f), clamp8(v * 0.75f), clamp8(v * 0.45f));
        }
    }
}

#define GHOUL_W 32
#define GHOUL_H 48

typedef struct {
    uint8_t r, g, b;
} Rgb;

static const Rgb GHOUL_SKIN  = {120, 132, 108}; /* sickly grey-green */
static const Rgb GHOUL_DARK  = {48, 52, 44};    /* shadowed legs, ribs */
static const Rgb GHOUL_LIMB  = {92, 100, 82};   /* arms, a shade below skin */
static const Rgb GHOUL_CLAW  = {210, 200, 170}; /* bone-white fingertips */
static const Rgb GHOUL_EYE   = {255, 220, 60};  /* glowing yellow */
static const Rgb GHOUL_MOUTH = {40, 6, 6};
static const Rgb GHOUL_BLOOD = {150, 12, 10};

/* A texture holding equal-size frames side by side (sprite sheets) */
typedef struct {
    Texture *tex;
    int frame_w;
    int frame_h;
} Sheet;

/* Allocate a frames-wide sheet, every pixel the color key */
static Sheet sheet_alloc(Texture *tex, int frame_w, int frame_h, int frames) {
    tex_alloc(tex, frame_w * frames, frame_h);
    for (int y = 0; y < frame_h; y++) {
        for (int x = 0; x < tex->width; x++) {
            tex_set_key(tex, x, y);
        }
    }
    return (Sheet){tex, frame_w, frame_h};
}

/* Paint into frame `f` of a sheet; out-of-frame pixels are clipped.
 * Skin gets per-pixel noise so flat shapes read as flesh. */
static void sheet_px(const Sheet *sh, int f, int x, int y, Rgb c, float noise) {
    if (x < 0 || x >= sh->frame_w || y < 0 || y >= sh->frame_h) {
        return;
    }
    float k = 1.0f - noise * noise01(f * 97 + x * 7, y * 13);
    tex_set(sh->tex, f * sh->frame_w + x, y, clamp8(c.r * k), clamp8(c.g * k), clamp8(c.b * k));
}

static void sheet_rect(const Sheet *sh, int f, int x0, int y0, int x1, int y1, Rgb c, float noise) {
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            sheet_px(sh, f, x, y, c, noise);
        }
    }
}

static void sheet_ellipse(const Sheet *sh, int f, float cx, float cy, float rx, float ry,
                          Rgb c, float noise) {
    for (int y = (int)(cy - ry); y <= (int)(cy + ry); y++) {
        for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
            float dx = (x + 0.5f - cx) / rx;
            float dy = (y + 0.5f - cy) / ry;
            if (dx * dx + dy * dy <= 1.0f) {
                sheet_px(sh, f, x, y, c, noise);
            }
        }
    }
}

/* Head with glowing eyes and mouth; tilt shifts it sideways (pain) */
static void ghoul_head(const Sheet *sh, int f, int tilt, bool mouth_open) {
    int cx = 16 + tilt;
    sheet_ellipse(sh, f, (float)cx, 8, 5, 6, GHOUL_SKIN, 0.3f);
    sheet_rect(sh, f, cx - 3, 6, cx - 1, 8, GHOUL_EYE, 0);
    sheet_rect(sh, f, cx + 1, 6, cx + 3, 8, GHOUL_EYE, 0);
    sheet_rect(sh, f, cx - 2, 11, cx + 2, mouth_open ? 14 : 12, GHOUL_MOUTH, 0);
}

/* Gaunt torso, arms and legs. arm_lift raises the arms (attack);
 * stride spreads the legs (walk). */
static void ghoul_body(const Sheet *sh, int f, int arm_lift, int stride) {
    sheet_rect(sh, f, 11, 14, 21, 30, GHOUL_SKIN, 0.35f);          /* torso */
    sheet_rect(sh, f, 13, 18, 19, 19, GHOUL_DARK, 0);              /* ribs */
    sheet_rect(sh, f, 13, 21, 19, 22, GHOUL_DARK, 0);
    sheet_rect(sh, f, 7, 15 - arm_lift, 10, 30 - arm_lift, GHOUL_LIMB, 0.3f);  /* arms */
    sheet_rect(sh, f, 22, 15 - arm_lift, 25, 30 - arm_lift, GHOUL_LIMB, 0.3f);

    /* Claws at the hand end: top when raised, bottom when hanging */
    int claw_y = arm_lift > 0 ? 13 - arm_lift : 30;
    sheet_rect(sh, f, 7, claw_y, 10, claw_y + 2, GHOUL_CLAW, 0.2f);
    sheet_rect(sh, f, 22, claw_y, 25, claw_y + 2, GHOUL_CLAW, 0.2f);
    sheet_rect(sh, f, 12 - stride, 30, 15 - stride, 48, GHOUL_DARK, 0.3f);    /* legs */
    sheet_rect(sh, f, 17 + stride, 30, 20 + stride, 48, GHOUL_DARK, 0.3f);
}

/* Five-frame sheet, rows top (y=0) to bottom:
 *
 *   STAND   WALK    ATTACK   PAIN    DEAD
 *    o       o      \o/      o/
 *   /|\     /|\      |      /|\
 *   / \    /   \    / \     / \     ___o_
 */
static void gen_ghoul(Texture *tex) {
    Sheet sh = sheet_alloc(tex, GHOUL_W, GHOUL_H, GHOUL_FRAME_COUNT);

    ghoul_body(&sh, GHOUL_FRAME_STAND, 0, 0);
    ghoul_head(&sh, GHOUL_FRAME_STAND, 0, false);

    ghoul_body(&sh, GHOUL_FRAME_WALK, 0, 3);
    ghoul_head(&sh, GHOUL_FRAME_WALK, 0, false);

    ghoul_body(&sh, GHOUL_FRAME_ATTACK, 10, 1);
    ghoul_head(&sh, GHOUL_FRAME_ATTACK, 0, true);

    ghoul_body(&sh, GHOUL_FRAME_PAIN, 0, 0);
    ghoul_head(&sh, GHOUL_FRAME_PAIN, 3, true);
    sheet_ellipse(&sh, GHOUL_FRAME_PAIN, 15, 20, 3, 3, GHOUL_BLOOD, 0.3f);

    /* Corpse: a heap on the floor in a pool of blood */
    sheet_ellipse(&sh, GHOUL_FRAME_DEAD, 16, 46, 15, 2, GHOUL_BLOOD, 0.3f);
    sheet_ellipse(&sh, GHOUL_FRAME_DEAD, 15, 43, 11, 3, GHOUL_SKIN, 0.4f);
    sheet_ellipse(&sh, GHOUL_FRAME_DEAD, 26, 42, 3, 3, GHOUL_SKIN, 0.3f);
    sheet_rect(&sh, GHOUL_FRAME_DEAD, 8, 44, 18, 45, GHOUL_DARK, 0);
}

/* --- Viewmodels: the held weapon, seen from behind; muzzle points
 * up-screen. Frame 1 adds the muzzle flash. --- */

static const Rgb VIEW_SKIN   = {140, 104, 84};
static const Rgb VIEW_METAL  = {58, 58, 64};
static const Rgb VIEW_SHINE  = {104, 104, 112};
static const Rgb VIEW_BORE   = {12, 12, 14};
static const Rgb VIEW_WOOD   = {104, 66, 38};
static const Rgb FLASH_OUTER = {255, 196, 96};
static const Rgb FLASH_INNER = {255, 250, 220};

static void muzzle_flash(const Sheet *sh, float cx, float cy, float size) {
    sheet_ellipse(sh, VIEW_FRAME_FIRE, cx, cy, size * 1.4f, size, FLASH_OUTER, 0.25f);
    sheet_ellipse(sh, VIEW_FRAME_FIRE, cx, cy, size * 0.6f, size * 0.45f, FLASH_INNER, 0);
}

/*   ▄▄       barrel + bore
 *  ████      slide (shine on top)
 *  ████
 * (hand)     grip hand at the bottom edge */
static void gen_view_pistol(Texture *tex) {
    Sheet sh = sheet_alloc(tex, VIEW_FRAME_W, VIEW_FRAME_H, VIEW_FRAME_COUNT);
    for (int f = 0; f < VIEW_FRAME_COUNT; f++) {
        sheet_rect(&sh, f, 26, 26, 38, 42, VIEW_METAL, 0.2f);     /* frame, near: wide */
        sheet_rect(&sh, f, 28, 18, 36, 26, VIEW_METAL, 0.2f);     /* slide, far: narrower */
        sheet_rect(&sh, f, 28, 18, 36, 20, VIEW_SHINE, 0.1f);
        sheet_rect(&sh, f, 31, 18, 33, 20, VIEW_BORE, 0);
        sheet_ellipse(&sh, f, 32, 45, 9, 5, VIEW_SKIN, 0.3f);     /* gripping hand */
        sheet_rect(&sh, f, 37, 34, 40, 41, VIEW_SKIN, 0.3f);      /* thumb up the side */
    }
    muzzle_flash(&sh, 32, 11, 6);
}

/* Long tapering barrel up the middle, wooden pump held by the left
 * hand, the right hand on the grip below */
static void gen_view_shotgun(Texture *tex) {
    Sheet sh = sheet_alloc(tex, VIEW_FRAME_W, VIEW_FRAME_H, VIEW_FRAME_COUNT);
    for (int f = 0; f < VIEW_FRAME_COUNT; f++) {
        sheet_rect(&sh, f, 27, 30, 37, 44, VIEW_METAL, 0.2f);     /* receiver, near */
        sheet_rect(&sh, f, 29, 8, 35, 30, VIEW_METAL, 0.2f);      /* barrel, far */
        sheet_rect(&sh, f, 30, 8, 34, 10, VIEW_SHINE, 0.1f);
        sheet_rect(&sh, f, 31, 8, 33, 10, VIEW_BORE, 0);
        sheet_rect(&sh, f, 26, 20, 38, 29, VIEW_WOOD, 0.3f);      /* pump */
        sheet_rect(&sh, f, 26, 22, 38, 23, VIEW_BORE, 0);         /* pump grooves */
        sheet_rect(&sh, f, 26, 25, 38, 26, VIEW_BORE, 0);
        sheet_ellipse(&sh, f, 32, 30, 8, 3, VIEW_SKIN, 0.3f);     /* left hand under the pump */
        sheet_ellipse(&sh, f, 34, 46, 9, 4, VIEW_SKIN, 0.3f);     /* right hand on the grip */
    }
    muzzle_flash(&sh, 32, 4, 7);
}

static const Rgb VIEW_OLIVE  = {62, 70, 48};

/* Fat tube on the shoulder: wide near end, rim and dark bore at the far
 * end, a hand on the side grip */
static void gen_view_launcher(Texture *tex) {
    Sheet sh = sheet_alloc(tex, VIEW_FRAME_W, VIEW_FRAME_H, VIEW_FRAME_COUNT);
    for (int f = 0; f < VIEW_FRAME_COUNT; f++) {
        sheet_rect(&sh, f, 22, 28, 44, 48, VIEW_OLIVE, 0.25f);    /* near end */
        sheet_rect(&sh, f, 25, 12, 41, 28, VIEW_OLIVE, 0.25f);    /* tube */
        sheet_rect(&sh, f, 25, 12, 41, 15, VIEW_SHINE, 0.1f);     /* rim */
        sheet_ellipse(&sh, f, 33, 14, 5, 2, VIEW_BORE, 0);
        sheet_rect(&sh, f, 22, 34, 44, 36, VIEW_METAL, 0.1f);     /* band */
        sheet_ellipse(&sh, f, 19, 42, 6, 5, VIEW_SKIN, 0.3f);     /* hand on the grip */
    }
    muzzle_flash(&sh, 33, 6, 9);
}

static const Rgb ROCKET_BODY  = {150, 150, 140};
static const Rgb ROCKET_FIRE  = {255, 170, 60};
static const Rgb SPIT_OUTER   = {90, 200, 40};
static const Rgb SPIT_INNER   = {220, 255, 140};

/* Small glowing projectiles seen head-on: a rocket (grey nose ringed by
 * exhaust fire) and a spit glob */
static void gen_projectiles(Texture *tex) {
    Sheet sh = sheet_alloc(tex, PROJ_FRAME_SIZE, PROJ_FRAME_SIZE, PROJ_FRAME_COUNT);
    float c = PROJ_FRAME_SIZE * 0.5f;
    sheet_ellipse(&sh, PROJ_FRAME_ROCKET, c, c, 7, 7, ROCKET_FIRE, 0.4f);
    sheet_ellipse(&sh, PROJ_FRAME_ROCKET, c, c, 4, 4, ROCKET_BODY, 0.2f);
    sheet_ellipse(&sh, PROJ_FRAME_SPIT, c, c, 6, 6, SPIT_OUTER, 0.35f);
    sheet_ellipse(&sh, PROJ_FRAME_SPIT, c, c, 3, 3, SPIT_INNER, 0.1f);
}

/* --- Public API --- */

static const char *proctex_names[PTEX_COUNT] = {
    "concrete", "brick", "metal_panel", "metal_grate",
    "rust", "floor_tile", "pipe", "barrel", "ghoul",
    "view_pistol", "view_shotgun", "view_launcher", "projectiles"
};

static void (*proctex_generators[PTEX_COUNT])(Texture *) = {
    gen_concrete, gen_brick, gen_metal_panel, gen_metal_grate,
    gen_rust, gen_floor_tile, gen_pipe, gen_barrel, gen_ghoul,
    gen_view_pistol, gen_view_shotgun, gen_view_launcher, gen_projectiles
};

const char *proctex_name(ProceduralTextureID id) {
    if (id < 0 || id >= PTEX_COUNT) {
        return NULL;
    }
    return proctex_names[id];
}

int proctex_find(const char *name) {
    if (!name || strcmp(name, "none") == 0) {
        return -1;
    }
    for (int i = 0; i < PTEX_COUNT; i++) {
        if (strcmp(name, proctex_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

void proctex_generate(Texture *tex, ProceduralTextureID id) {
    if (id >= 0 && id < PTEX_COUNT) {
        proctex_generators[id](tex);
    }
}

void proctex_generate_all(Texture *textures) {
    for (int i = 0; i < PTEX_COUNT; i++) {
        proctex_generators[i](&textures[i]);
    }
}
