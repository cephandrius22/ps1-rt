/*
 * proctex_church.c — Church and crypt surfaces: stone, wood, plaster,
 * marble, carpet, iron and stained glass.
 *
 * Same rules as proctex.c: tiny textures from hash noise, nothing loaded.
 * The tiling ones are 64x64 (2 m in TrenchBroom) and repeat seamlessly;
 * tomb_slab and rose_window are single pieces sized to fill one face:
 *
 *   tiling:   ashlar rubble flagstone plaster oak_panel oak_planks
 *             marble carpet iron stained_glass
 *   pieces:   tomb_slab (1 x 2 m)   rose_window (2 x 2 m)
 *   sprites:  candelabra gravestone statue (color-keyed, one frame;
 *             40 px = 1 m, the barrel's scale)
 */
#include "proctex_internal.h"
#include <math.h>
#include <stdbool.h>

#define CH         64                       /* tiling church texture size */
#define TWO_PI     (2.0f * (float)MATH_PI)

/* --- Palette --- */

static const Rgb LIMESTONE = {150, 140, 120};
static const Rgb MORTAR    = {62, 58, 52};
static const Rgb RUBBLE    = {104, 98, 88};
static const Rgb MOSS      = {54, 76, 36};
static const Rgb SLATE     = {84, 84, 88};
static const Rgb TOMB      = {104, 104, 108};
static const Rgb PLASTER   = {176, 164, 138};
static const Rgb DAMP      = {108, 88, 60};
static const Rgb OAK       = {96, 60, 32};
static const Rgb OAK_DARK  = {34, 20, 10};
static const Rgb MARBLE    = {196, 192, 182};
static const Rgb VEIN      = {70, 78, 74};
static const Rgb CARPET    = {124, 18, 24};
static const Rgb GOLD      = {176, 136, 52};
static const Rgb IRON      = {56, 58, 64};
static const Rgb RUST      = {112, 54, 22};
static const Rgb LEAD      = {18, 18, 20};

typedef enum {
    GLASS_RUBY, GLASS_COBALT, GLASS_AMBER, GLASS_GREEN, GLASS_VIOLET,
    GLASS_COUNT
} GlassColor;

static const Rgb GLASS[GLASS_COUNT] = {
    [GLASS_RUBY]   = {176, 22, 34},
    [GLASS_COBALT] = {32, 58, 180},
    [GLASS_AMBER]  = {214, 150, 36},
    [GLASS_GREEN]  = {38, 132, 60},
    [GLASS_VIOLET] = {104, 40, 146},
};

/* --- Helpers --- */

static int wrap(int a, int n) {
    return ((a % n) + n) % n;
}

static float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

static float clamp01(float v) {
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}

/* Palette color c scaled by k (1 = as listed) */
static void put(Texture *tex, int x, int y, Rgb c, float k) {
    tex_set(tex, x, y, clamp8(c.r * k), clamp8(c.g * k), clamp8(c.b * k));
}

/* a blended toward b by t (0..1) */
static Rgb mix(Rgb a, Rgb b, float t) {
    t = clamp01(t);
    return (Rgb){clamp8(lerpf(a.r, b.r, t)), clamp8(lerpf(a.g, b.g, t)), clamp8(lerpf(a.b, b.b, t))};
}

/* Smooth value noise in 0..1 that repeats every `period` pixels: random
 * values on a lattice every `cell` pixels (period a multiple of cell),
 * blended in between. Stains, veins and wood-grain wobble use it. */
static float tiled_noise(int x, int y, int cell, int period, int salt) {
    int n = period / cell;
    x = wrap(x, period);
    y = wrap(y, period);
    int cx = x / cell;
    int cy = y / cell;
    float tx = (float)(x - cx * cell) / cell;
    float ty = (float)(y - cy * cell) / cell;
    tx = tx * tx * (3 - 2 * tx);
    ty = ty * ty * (3 - 2 * ty);

    int nx = (cx + 1) % n;
    int ny = (cy + 1) % n;
    float a = noise01(cx + salt, cy + salt * 3);
    float b = noise01(nx + salt, cy + salt * 3);
    float c = noise01(cx + salt, ny + salt * 3);
    float d = noise01(nx + salt, ny + salt * 3);
    return lerpf(lerpf(a, b, tx), lerpf(c, d, tx), ty);
}

/* Two octaves: broad blotches plus finer detail */
static float tiled_fbm(int x, int y, int salt) {
    return tiled_noise(x, y, 16, CH, salt) * 0.65f + tiled_noise(x, y, 8, CH, salt + 101) * 0.35f;
}

typedef bool CrackMask[CH][CH];

/* Mark a crack: walk len pixels from (x, y) along (dx, dy), now and then
 * stepping a pixel sideways; wraps with the tile. Returns where it ended
 * so a branch can start there. */
static void crack_walk(CrackMask mask, int *x, int *y, int dx, int dy, int len, int salt) {
    for (int i = 0; i < len; i++) {
        mask[wrap(*y, CH)][wrap(*x, CH)] = true;
        float r = noise01(i * 31 + salt, salt * 7);
        int side = r < 0.25f ? -1 : (r > 0.75f ? 1 : 0);
        *x += dx - side * dy;
        *y += dy + side * dx;
    }
}

/* A crack with one branch splitting off partway */
static void crack(CrackMask mask, int x, int y, int dx, int dy, int len, int salt) {
    crack_walk(mask, &x, &y, dx, dy, len / 2, salt);
    int bx = x;
    int by = y;
    crack_walk(mask, &x, &y, dx, dy, len - len / 2, salt + 1);
    crack_walk(mask, &bx, &by, dx + dy, dy + dx, len / 3, salt + 2);
}

/* --- Stone --- */

#define ASHLAR_W 32   /* dressed block: 1 m x 0.5 m, rows offset by half */
#define ASHLAR_H 16

/* Dressed limestone blocks: the nave walls and pillars */
void church_ashlar(Texture *tex) {
    tex_alloc(tex, CH, CH);
    for (int y = 0; y < CH; y++) {
        int row = y / ASHLAR_H;
        int in_y = y % ASHLAR_H;
        for (int x = 0; x < CH; x++) {
            int bx = wrap(x + (row % 2) * ASHLAR_W / 2, CH);
            int col = bx / ASHLAR_W;
            int in_x = bx % ASHLAR_W;

            /* Mortar joint along the top and left of each block */
            if (in_x == 0 || in_y == 0) {
                put(tex, x, y, MORTAR, 0.8f + noise01(x, y) * 0.3f);
                continue;
            }

            /* Block tone, chisel marks, soot and damp */
            float k = 0.78f + noise01(col * 11 + 5, row * 17 + 3) * 0.22f +
                      (noise01(x * 3, y * 3) - 0.5f) * 0.10f - tiled_fbm(x, y, 7) * 0.25f;

            /* Arrises catch light on top, shade below and right */
            if (in_y == 1) {
                k += 0.08f;
            }
            if (in_y == ASHLAR_H - 1 || in_x == ASHLAR_W - 1) {
                k -= 0.10f;
            }

            /* Chips knocked out of the edges, a few pixels at a time */
            bool edge = in_x == 1 || in_y == 1 || in_x == ASHLAR_W - 1 || in_y == ASHLAR_H - 1;
            if (edge && noise01(x / 3 + 900, y / 3 + 900) > 0.8f) {
                k -= 0.2f;
            }
            put(tex, x, y, LIMESTONE, k);
        }
    }
}

#define RUBBLE_CELLS  4      /* stones per tile side, one per 16 px cell */
#define RUBBLE_SQUASH 1.35f  /* stones lie flatter than they are wide */
#define RUBBLE_JOINT  1.8f   /* px: mortar where two stones are this close */

/* Jittered center of stone (cx, cy); cells outside the tile wrap, so
 * the pattern repeats */
static void rubble_center(int cx, int cy, float *px, float *py) {
    int wx = wrap(cx, RUBBLE_CELLS);
    int wy = wrap(cy, RUBBLE_CELLS);
    float cell = (float)CH / RUBBLE_CELLS;
    *px = (cx + 0.2f + noise01(wx * 5 + 1, wy * 7 + 2) * 0.6f) * cell;
    *py = (cy + 0.2f + noise01(wx * 3 + 9, wy * 11 + 4) * 0.6f) * cell;
}

/* Rough fieldstone set in mortar, damp and mossy: crypt walls.
 * Each pixel belongs to its nearest stone center (Voronoi cells):
 *
 *     .--.  .---.        d1 = distance to nearest center
 *    ( a  )(  b  )       d2 - d1 small → between stones → mortar
 *     '--'  '---'
 */
void church_rubble(Texture *tex) {
    float cell = (float)CH / RUBBLE_CELLS;
    tex_alloc(tex, CH, CH);
    for (int y = 0; y < CH; y++) {
        for (int x = 0; x < CH; x++) {
            int cx = (int)(x / cell);
            int cy = (int)(y / cell);
            float d1 = 1e9f;
            float d2 = 1e9f;
            int id = 0;
            for (int j = -1; j <= 1; j++) {
                for (int i = -1; i <= 1; i++) {
                    float px, py;
                    rubble_center(cx + i, cy + j, &px, &py);
                    float dx = x + 0.5f - px;
                    float dy = (y + 0.5f - py) * RUBBLE_SQUASH;
                    float d = sqrtf(dx * dx + dy * dy);
                    if (d < d1) {
                        d2 = d1;
                        d1 = d;
                        id = wrap(cx + i, RUBBLE_CELLS) + wrap(cy + j, RUBBLE_CELLS) * RUBBLE_CELLS;
                    } else if (d < d2) {
                        d2 = d;
                    }
                }
            }

            if (d2 - d1 < RUBBLE_JOINT) {
                put(tex, x, y, MORTAR, 0.55f + noise01(x, y) * 0.3f);
                continue;
            }

            /* Domed stones: darker toward their rims; moss in the damp */
            float k = 0.75f + noise01(id * 13, id * 7 + 1) * 0.3f - (d1 / cell) * 0.35f +
                      (noise01(x * 5, y * 5) - 0.5f) * 0.12f;
            float moss = (tiled_fbm(x, y, 19) - 0.55f) * 3.0f;
            put(tex, x, y, mix(RUBBLE, MOSS, moss), k);
        }
    }
}

#define FLAG_ROW 32   /* two rows of floor slabs per tile, 1 m deep */

/* Joints between slabs in each row (x px; each row is two slabs) */
static const int FLAG_JOINTS[CH / FLAG_ROW][2] = {{0, 40}, {20, 52}};

/* Worn floor slabs, polished down the middle by feet; some cracked */
void church_flagstone(Texture *tex) {
    CrackMask cracks = {{false}};
    for (int row = 0; row < CH / FLAG_ROW; row++) {
        for (int s = 0; s < 2; s++) {
            int salt = row * 2 + s;
            if (noise01(salt, 77) < 0.45f) {
                continue;
            }
            int x0 = FLAG_JOINTS[row][s] + 6 + (int)(noise01(salt, 79) * 14);
            crack(cracks, x0, row * FLAG_ROW + 1, 0, 1, 14 + (int)(noise01(salt, 81) * 14), 300 + salt * 10);
        }
    }

    tex_alloc(tex, CH, CH);
    for (int y = 0; y < CH; y++) {
        int row = y / FLAG_ROW;
        int a = FLAG_JOINTS[row][0];
        int b = FLAG_JOINTS[row][1];
        for (int x = 0; x < CH; x++) {
            if (y % FLAG_ROW == 0 || x == a || x == b) {
                put(tex, x, y, MORTAR, 0.5f + noise01(x, y) * 0.3f);
                continue;
            }

            int slab = row * 2 + (x > a && x < b ? 0 : 1);
            float k = 0.8f + noise01(slab * 29, slab * 31 + 3) * 0.25f +
                      tiled_noise(x, y, 32, CH, 23) * 0.15f - tiled_fbm(x, y, 37) * 0.2f +
                      (noise01(x * 3, y * 3) - 0.5f) * 0.08f;

            if (cracks[y][x]) {
                k *= 0.45f;
            }
            put(tex, x, y, SLATE, k);
        }
    }
}

#define TOMB_W       32   /* 1 m x 2 m: a tomb lid or floor memorial */
#define TOMB_H       64
#define TOMB_INSET   3    /* engraved border line */
#define TOMB_TEXT_Y  34   /* first line of the inscription */
#define TOMB_LINES   5
#define TOMB_LINE_DY 5

/* Is (x, y) cut into the slab? Border, a cross, then lines of lettering */
static bool tomb_engraved(int x, int y) {
    bool border_x = (x == TOMB_INSET || x == TOMB_W - 1 - TOMB_INSET) &&
                    y >= TOMB_INSET && y <= TOMB_H - 1 - TOMB_INSET;
    bool border_y = (y == TOMB_INSET || y == TOMB_H - 1 - TOMB_INSET) &&
                    x >= TOMB_INSET && x <= TOMB_W - 1 - TOMB_INSET;
    if (border_x || border_y) {
        return true;
    }

    /* Cross: upright x 15-16, y 8-27; arm y 13-14, x 10-21 */
    if ((x == 15 || x == 16) && y >= 8 && y <= 27) {
        return true;
    }
    if ((y == 13 || y == 14) && x >= 10 && x <= 21) {
        return true;
    }

    /* Lettering: lines of 2 px tall glyph specks, each line centered and
     * shorter or longer (a name, dates, a verse) */
    for (int line = 0; line < TOMB_LINES; line++) {
        int top = TOMB_TEXT_Y + line * TOMB_LINE_DY;
        if (y < top || y > top + 1) {
            continue;
        }
        int half = 6 + (int)(noise01(line, 5) * 5);
        if (x < TOMB_W / 2 - half || x >= TOMB_W / 2 + half) {
            return false;
        }
        return noise01(x / 2 + line * 17, 3) > 0.35f && x % 2 == 0;
    }
    return false;
}

/* Engraved memorial slab; light from above catches each cut's lower lip */
void church_tomb_slab(Texture *tex) {
    tex_alloc(tex, TOMB_W, TOMB_H);
    for (int y = 0; y < TOMB_H; y++) {
        for (int x = 0; x < TOMB_W; x++) {
            float k = 0.85f + (noise01(x * 3, y * 3) - 0.5f) * 0.1f - tiled_fbm(x, y, 43) * 0.25f;
            if (tomb_engraved(x, y)) {
                k *= 0.4f;
            } else if (y > 0 && tomb_engraved(x, y - 1)) {
                k += 0.12f;
            }

            /* Edge of the slab: a dark gap to the floor around it */
            if (x == 0 || y == 0 || x == TOMB_W - 1 || y == TOMB_H - 1) {
                k *= 0.35f;
            }
            put(tex, x, y, TOMB, k);
        }
    }
}

/* --- Plaster, wood --- */

#define DAMP_LEVEL 0.58f  /* stain where the damp noise rises above this */

/* Yellowed lime plaster with damp blooms, tide lines, cracks and
 * patches flaked off to the stone beneath */
void church_plaster(Texture *tex) {
    CrackMask cracks = {{false}};
    crack(cracks, 12, 4, 0, 1, 30, 400);
    crack(cracks, 46, 30, 1, 0, 24, 410);

    tex_alloc(tex, CH, CH);
    for (int y = 0; y < CH; y++) {
        for (int x = 0; x < CH; x++) {
            float k = 0.85f + (noise01(x, y) - 0.5f) * 0.08f;
            float damp = tiled_fbm(x, y, 31);
            Rgb c = mix(PLASTER, DAMP, (damp - DAMP_LEVEL) * 3.0f);

            /* Faint brown tide line round each stain */
            if (fabsf(damp - DAMP_LEVEL) < 0.015f) {
                c = mix(c, DAMP, 0.6f);
            }

            /* Flaked patches show the stone, shadowed on top by the edge */
            float flake = tiled_noise(x, y, 8, CH, 61);
            if (flake > 0.86f) {
                c = LIMESTONE;
                k = 0.55f + noise01(x * 3, y * 3) * 0.1f;
                if (tiled_noise(x, y - 1, 8, CH, 61) <= 0.86f) {
                    k = 0.35f;
                }
            }

            if (cracks[y][x]) {
                k *= 0.5f;
            }
            put(tex, x, y, c, k);
        }
    }
}

#define GRAIN_STREAKS 8   /* grain lines across a 64 px board */

/* Wood grain, 0..1: streaks running along `along`, wobbling across.
 * Whole sine periods per tile, so boards repeat seamlessly. */
static float grain(int along, int across, int salt) {
    float wobble = tiled_noise(along, across, 16, CH, salt) * 6.0f;
    return sinf((across + wobble) * TWO_PI * GRAIN_STREAKS / CH) * 0.5f + 0.5f;
}

#define PANEL       32   /* one framed panel, 1 m square */
#define PANEL_FRAME 4    /* rail and stile width */
#define PANEL_BEVEL 3    /* raised field's sloped edge */

/* Dark oak wainscot: framed raised panels. Per 32 px cell:
 *
 *   ┌────────── rail (grain across) ┐
 *   │ s ┌──── bevel ─────┐          │
 *   │ t │   raised field │          │
 *   │ i │  (grain down)  │          │
 *   │ l └────────────────┘          │
 *   │ e                             │
 */
void church_oak_panel(Texture *tex) {
    tex_alloc(tex, CH, CH);
    for (int y = 0; y < CH; y++) {
        for (int x = 0; x < CH; x++) {
            int in_x = x % PANEL;
            int in_y = y % PANEL;

            /* Frame: rails along the top, stiles down the left */
            if (in_y < PANEL_FRAME || in_x < PANEL_FRAME) {
                bool rail = in_y < PANEL_FRAME;
                float g = rail ? grain(x, y, 3) : grain(y, x, 5);
                float k = 0.75f + g * 0.2f + noise01(x, y) * 0.05f;
                if ((rail && in_y == 0) || (!rail && in_x == 0)) {
                    k *= 0.6f; /* joint to the next frame piece */
                }
                put(tex, x, y, OAK, k);
                continue;
            }

            /* Field: bevel shaded by which way it slopes (light from above) */
            float k = 0.8f + grain(y, x, 9) * 0.25f + noise01(x, y) * 0.05f;
            int to_left = in_x - PANEL_FRAME;
            int to_top = in_y - PANEL_FRAME;
            int to_right = PANEL - 1 - in_x;
            int to_bottom = PANEL - 1 - in_y;
            int nearest = to_left;
            float slope = -0.04f;
            if (to_top < nearest) {
                nearest = to_top;
                slope = 0.12f;
            }
            if (to_right < nearest) {
                nearest = to_right;
                slope = -0.12f;
            }
            if (to_bottom < nearest) {
                nearest = to_bottom;
                slope = -0.2f;
            }
            if (nearest == 0) {
                put(tex, x, y, OAK_DARK, 1.0f);
                continue;
            }
            if (nearest < PANEL_BEVEL) {
                k += slope;
            }
            put(tex, x, y, OAK, k);
        }
    }
}

#define BOARD_H 8   /* 25 cm boards, two butt joints per row */

/* Plain oak boards: pews, doors, floors, beams. Grain runs along x */
void church_oak_planks(Texture *tex) {
    tex_alloc(tex, CH, CH);
    for (int y = 0; y < CH; y++) {
        int row = y / BOARD_H;
        int in_y = y % BOARD_H;
        int j1 = (int)(noise01(row, 11) * CH);
        int j2 = wrap(j1 + CH / 2 - 8 + (int)(noise01(row, 13) * 16), CH);
        for (int x = 0; x < CH; x++) {
            if (in_y == 0 || x == j1 || x == j2) {
                put(tex, x, y, OAK_DARK, 1.0f + noise01(x, y) * 0.3f);
                continue;
            }

            /* Segment between joints sets the board's tone */
            int seg = wrap(x - j1, CH) < wrap(j2 - j1, CH) ? 0 : 1;
            float k = 0.7f + noise01(row * 7 + seg, 19) * 0.25f + grain(x, y, 17) * 0.22f +
                      noise01(x, y) * 0.05f;

            /* Nails beside each joint */
            bool nail_x = x == wrap(j1 + 2, CH) || x == wrap(j2 + 2, CH);
            if (nail_x && (in_y == 2 || in_y == BOARD_H - 2)) {
                k = 0.3f;
            }
            put(tex, x, y, OAK, k);
        }
    }
}

/* --- Marble, carpet, iron --- */

/* White marble with grey-green veins (altar, steps, columns). Veins are
 * the thin bands where a turbulent sine is near zero; whole periods per
 * tile keep it seamless. */
void church_marble(Texture *tex) {
    tex_alloc(tex, CH, CH);
    for (int y = 0; y < CH; y++) {
        for (int x = 0; x < CH; x++) {
            float main_phase = (x + y) * TWO_PI / CH + tiled_fbm(x, y, 71) * 5.0f;
            float fine_phase = (x - y) * TWO_PI * 2 / CH + tiled_fbm(x, y, 83) * 4.0f;
            float vein = clamp01(1.0f - fabsf(sinf(main_phase)) / 0.22f);
            float fine = clamp01(1.0f - fabsf(sinf(fine_phase)) / 0.2f) * 0.25f;

            float k = 0.88f + (tiled_fbm(x, y, 97) - 0.5f) * 0.14f + noise01(x, y) * 0.04f;
            put(tex, x, y, mix(MARBLE, VEIN, fmaxf(vein * vein, fine) * 0.8f), k);
        }
    }
}

#define CARPET_BORDER 6    /* gold border down each side (a runner) */
#define CARPET_MOTIF  16   /* diamond lattice period */

/* Red runner for the aisle: gold border, diamond lattice, worn thin
 * down the middle. Runs along v. */
void church_carpet(Texture *tex) {
    tex_alloc(tex, CH, CH);
    for (int y = 0; y < CH; y++) {
        for (int x = 0; x < CH; x++) {
            float wear = noise01(x, y) * 0.15f - tiled_fbm(x, y, 113) * 0.2f;
            int edge = x < CH / 2 ? x : CH - 1 - x;

            /* Border: dark rims, gold band with a dotted inner line */
            if (edge < CARPET_BORDER) {
                bool rim = edge == 0 || edge == CARPET_BORDER - 1;
                bool dot = edge == 2 && y % 4 == 0;
                put(tex, x, y, rim || dot ? CARPET : GOLD, (rim ? 0.5f : 0.85f) + wear);
                continue;
            }

            /* Field: |dx| + |dy| diamonds; gold outline and a gold center */
            int dx = abs(wrap(x - CH / 2, CARPET_MOTIF) - CARPET_MOTIF / 2);
            int dy = abs(y % CARPET_MOTIF - CARPET_MOTIF / 2);
            int d = dx + dy;
            if (d == CARPET_MOTIF / 2 - 1 || d < 2) {
                put(tex, x, y, GOLD, 0.7f + wear);
                continue;
            }
            put(tex, x, y, CARPET, (d < 4 ? 0.65f : 0.85f) + wear);
        }
    }
}

#define STRAP_EVERY 32   /* a riveted strap every 1 m */
#define STRAP_H     5
#define STUD_EVERY  8
#define STUD_ROW    2    /* studs' row within the strap */
#define RUST_RUN    12   /* longest rust streak below a stud, px */

/* Studded iron: crypt gates, vault doors, strapping on oak doors */
void church_iron(Texture *tex) {
    tex_alloc(tex, CH, CH);
    for (int y = 0; y < CH; y++) {
        int in_strap = y % STRAP_EVERY;
        for (int x = 0; x < CH; x++) {
            /* Hammered plate: small smooth dimples */
            float k = 0.72f + tiled_noise(x, y, 4, CH, 131) * 0.2f + noise01(x, y) * 0.06f;
            Rgb c = IRON;

            if (in_strap < STRAP_H) {
                k += in_strap == 0 ? 0.25f : (in_strap == STRAP_H - 1 ? -0.2f : 0.12f);
            }

            /* Studs: lit top-left, shadowed bottom-right */
            int sx = x % STUD_EVERY;
            int sy = in_strap - STUD_ROW;
            if (sx >= 3 && sx <= 4 && sy >= 0 && sy <= 1) {
                k = sx == 3 && sy == 0 ? 1.4f : 0.9f;
            }

            /* Rust bleeding down from each stud */
            int stud = x / STUD_EVERY + (y / STRAP_EVERY) * (CH / STUD_EVERY);
            int run = 4 + (int)(noise01(stud, 151) * (RUST_RUN - 4));
            int below = in_strap - STUD_ROW - 2;
            if (sx >= 3 && sx <= 4 && below >= 0 && below < run) {
                c = mix(IRON, RUST, 0.8f * (1.0f - (float)below / run));
            }
            float patch = tiled_fbm(x, y, 157) + noise01(x * 3, y * 3) * 0.08f;
            if (patch > 0.74f) {
                c = mix(c, RUST, (patch - 0.74f) * 4.0f);
                k *= 0.85f;
            }
            put(tex, x, y, c, k);
        }
    }
}

/* --- Stained glass (matlib makes these glow) --- */

#define QUARRY 16   /* diamond panes: lead every 16 px along both diagonals */

/* Leaded quarry glazing: diamond panes in jewel colors.
 *
 *    \ /\ /\ /     lead where (x + y) or (x - y) is a multiple of QUARRY
 *     X  X  X      pane (a, b) = which diagonal strip it lies in;
 *    / \/ \/ \     color hashed from (a, b) mod the tile, so it repeats
 */
void church_stained_glass(Texture *tex) {
    int strips = CH / QUARRY;
    tex_alloc(tex, CH, CH);
    for (int y = 0; y < CH; y++) {
        for (int x = 0; x < CH; x++) {
            int a = x + y;
            int b = x - y + CH;
            if (a % QUARRY == 0 || b % QUARRY == 0) {
                put(tex, x, y, LEAD, 1.0f);
                continue;
            }
            int pa = (a / QUARRY) % strips;
            int pb = (b / QUARRY) % strips;
            Rgb c = GLASS[hash2d(pa + 7, pb + 3) % GLASS_COUNT];
            float k = 0.7f + tiled_noise(x, y, 8, CH, 167) * 0.25f + noise01(x, y) * 0.1f;
            put(tex, x, y, c, k);
        }
    }
}

#define ROSE_C      31.5f   /* center */
#define ROSE_HUB    6.0f    /* amber center roundel */
#define ROSE_RING   18.0f   /* inner petals end here */
#define ROSE_RIM    29.5f   /* glass ends here; stone tracery beyond */
#define ROSE_PETALS 12
#define ROSE_FOIL   4.0f    /* radius of the roundel in each outer petal */
#define ROSE_LEAD   0.7f    /* half-width of lead lines, px */

/* Rose window: a hub, a ring of 12 petals, and 12 roundels in the outer
 * ring, in a stone frame. One window per face.
 *
 *          .-~~~-.
 *        /  o o o  \      o  roundels (amber / green on violet)
 *       | o \ | / o |     \|/ petals (ruby / cobalt, alternating)
 *       | o -(*)- o |     *  hub (amber)
 *        \  o o o  /
 *          '-___-'
 */
void church_rose_window(Texture *tex) {
    float sector = TWO_PI / ROSE_PETALS;
    tex_alloc(tex, CH, CH);
    for (int y = 0; y < CH; y++) {
        for (int x = 0; x < CH; x++) {
            float dx = x - ROSE_C;
            float dy = y - ROSE_C;
            float r = sqrtf(dx * dx + dy * dy);
            float glass_k = 0.75f + noise01(x, y) * 0.2f;

            /* Stone frame around the glass */
            if (r > ROSE_RIM + ROSE_LEAD) {
                put(tex, x, y, LIMESTONE, 0.45f + noise01(x * 3, y * 3) * 0.12f);
                continue;
            }

            /* Lead: rings, then the spokes between petals */
            float ang = atan2f(dy, dx) + (float)MATH_PI;
            int petal = (int)(ang / sector) % ROSE_PETALS;
            float to_spoke = fminf(ang - petal * sector, (petal + 1) * sector - ang) * r;
            bool ring_lead = fabsf(r - ROSE_HUB) < ROSE_LEAD || fabsf(r - ROSE_RING) < ROSE_LEAD ||
                             fabsf(r - ROSE_RIM) < ROSE_LEAD;
            if (ring_lead || (r > ROSE_HUB && to_spoke < ROSE_LEAD)) {
                put(tex, x, y, LEAD, 1.0f);
                continue;
            }

            if (r < ROSE_HUB) {
                put(tex, x, y, GLASS[GLASS_AMBER], glass_k);
                continue;
            }
            if (r < ROSE_RING) {
                put(tex, x, y, GLASS[petal % 2 ? GLASS_COBALT : GLASS_RUBY], glass_k);
                continue;
            }

            /* Outer ring: a roundel in the middle of each petal */
            float mid = (petal + 0.5f) * sector - (float)MATH_PI;
            float fr = (ROSE_RING + ROSE_RIM) / 2;
            float fx = x - (ROSE_C + cosf(mid) * fr);
            float fy = y - (ROSE_C + sinf(mid) * fr);
            float fd = sqrtf(fx * fx + fy * fy);
            if (fabsf(fd - ROSE_FOIL) < ROSE_LEAD) {
                put(tex, x, y, LEAD, 1.0f);
                continue;
            }
            Rgb c = fd < ROSE_FOIL ? GLASS[petal % 2 ? GLASS_GREEN : GLASS_AMBER] : GLASS[GLASS_VIOLET];
            put(tex, x, y, c, glass_k);
        }
    }
}

/* --- Sprites (color-keyed, one frame; 40 px = 1 m) --- */

static const Rgb BRONZE     = {92, 68, 34};
static const Rgb BRONZE_LIT = {156, 118, 58};
static const Rgb WAX        = {226, 216, 190};
static const Rgb WAX_SHADE  = {168, 156, 128};
static const Rgb WICK       = {30, 24, 20};
static const Rgb FLAME_OUT  = {255, 150, 40};
static const Rgb FLAME_IN   = {255, 240, 180};
static const Rgb GRAVE      = {116, 116, 110};
static const Rgb ENGRAVED   = {46, 46, 44};
static const Rgb LICHEN     = {128, 126, 88};
static const Rgb GRASS      = {44, 70, 30};
static const Rgb GRASS_LIT  = {70, 102, 44};
static const Rgb EARTH      = {52, 40, 28};
static const Rgb STATUE     = {154, 150, 140};

/* c scaled by k, for shading flat sprite shapes */
static Rgb scaled(Rgb c, float k) {
    return (Rgb){clamp8(c.r * k), clamp8(c.g * k), clamp8(c.b * k)};
}

#define CANDELABRA_W 24
#define CANDELABRA_H 64

/* A candle: wax column, wick, and a teardrop flame above it. x0 is the
 * wax's left column, top its first row. */
static void candle(const Sheet *sh, int x0, int top, int bottom) {
    sheet_rect(sh, 0, x0, top, x0 + 4, bottom, WAX, 0.1f);
    sheet_rect(sh, 0, x0 + 3, top, x0 + 4, bottom, WAX_SHADE, 0.1f);
    sheet_px(sh, 0, x0 + 1, top + 2, WAX_SHADE, 0);          /* a drip */
    sheet_px(sh, 0, x0 + 1, top + 3, WAX_SHADE, 0);
    sheet_px(sh, 0, x0 + 2, top - 1, WICK, 0);

    float cx = x0 + 2.5f;
    sheet_ellipse(sh, 0, cx, top - 4.0f, 1.8f, 3.2f, FLAME_OUT, 0);
    sheet_ellipse(sh, 0, cx, top - 3.5f, 0.9f, 1.8f, FLAME_IN, 0);
}

/* Bronze three-branch candelabra on a tripod; the flames are lit by the
 * entity's own light (decor_candelabra). Rows, top down:
 *
 *        )            flames      y 2-12
 *    )  |#|  (        candles     side 13-20, center 8-16
 *   |#|=====|#|       drip pans, arms
 *        ||           shaft with two knops
 *       /||\          tripod      y 56-63
 */
void church_candelabra(Texture *tex) {
    Sheet sh = sheet_alloc(tex, CANDELABRA_W, CANDELABRA_H, 1);

    /* Tripod, shaft and knops */
    sheet_rect(&sh, 0, 6, 57, 18, 59, BRONZE, 0.2f);
    sheet_rect(&sh, 0, 3, 59, 7, 64, BRONZE, 0.2f);
    sheet_rect(&sh, 0, 17, 59, 21, 64, BRONZE, 0.2f);
    sheet_rect(&sh, 0, 11, 59, 13, 64, BRONZE, 0.2f);
    sheet_rect(&sh, 0, 11, 27, 13, 57, BRONZE, 0.2f);
    sheet_rect(&sh, 0, 11, 27, 12, 57, BRONZE_LIT, 0.2f);
    sheet_ellipse(&sh, 0, 12.0f, 35.5f, 2.5f, 1.6f, BRONZE, 0.2f);
    sheet_ellipse(&sh, 0, 12.0f, 47.5f, 3.0f, 2.0f, BRONZE, 0.2f);
    sheet_px(&sh, 0, 10, 47, BRONZE_LIT, 0);

    /* Arms: a crossbar, ends and center rising to the drip pans */
    sheet_rect(&sh, 0, 3, 26, 21, 28, BRONZE, 0.2f);
    sheet_rect(&sh, 0, 3, 26, 21, 27, BRONZE_LIT, 0.2f);
    sheet_rect(&sh, 0, 3, 22, 5, 27, BRONZE, 0.2f);
    sheet_rect(&sh, 0, 19, 22, 21, 27, BRONZE, 0.2f);
    sheet_rect(&sh, 0, 11, 18, 13, 27, BRONZE, 0.2f);
    sheet_rect(&sh, 0, 1, 21, 8, 23, BRONZE_LIT, 0.2f);
    sheet_rect(&sh, 0, 16, 21, 23, 23, BRONZE_LIT, 0.2f);
    sheet_rect(&sh, 0, 8, 17, 16, 19, BRONZE_LIT, 0.2f);

    candle(&sh, 2, 13, 21);
    candle(&sh, 18, 13, 21);
    candle(&sh, 10, 8, 17);
}

#define GRAVE_W   32
#define GRAVE_H   40
#define GRAVE_TOP 2    /* top of the rounded head */
#define GRAVE_FOOT 36  /* stone meets the ground */

/* Is (x, y) inside the headstone's outline (round-topped slab)? */
static bool grave_inside(int x, int y) {
    if (x < 4 || x > 27 || y > GRAVE_FOOT || y < GRAVE_TOP) {
        return false;
    }
    float dx = (x + 0.5f - 16.0f) / 12.0f;
    float dy = (y + 0.5f - 11.0f) / 9.0f;
    return y >= 11 || dx * dx + dy * dy <= 1.0f;
}

/* Is (x, y) cut into the face: a cross, then three lines of lettering? */
static bool grave_engraved(int x, int y) {
    bool cross = ((x == 15 || x == 16) && y >= 6 && y <= 17) ||
                 ((y == 9 || y == 10) && x >= 12 && x <= 19);
    bool text = (y == 21 || y == 24 || y == 27) && x >= 9 && x <= 22 &&
                x % 2 == 1 && noise01(x / 2, y) > 0.3f;
    return cross || text;
}

/* Weathered headstone in a tuft of grass, lichen creeping up */
void church_gravestone(Texture *tex) {
    Sheet sh = sheet_alloc(tex, GRAVE_W, GRAVE_H, 1);
    for (int y = 0; y < GRAVE_H; y++) {
        for (int x = 0; x < GRAVE_W; x++) {
            if (!grave_inside(x, y)) {
                continue;
            }

            /* Rounded edges: lit on the left, shaded on the right */
            float k = 0.85f;
            if (!grave_inside(x - 1, y)) {
                k = 1.1f;
            } else if (!grave_inside(x + 1, y) || !grave_inside(x + 2, y)) {
                k = 0.6f;
            }
            Rgb c = GRAVE;
            if (grave_engraved(x, y)) {
                c = ENGRAVED;
                k = 1.0f;
            } else if (noise01(x / 2 + 40, y / 2 + 40) > 0.92f - y * 0.004f) {
                c = LICHEN; /* thicker toward the ground */
            }
            sheet_px(&sh, 0, x, y, scaled(c, k), 0.2f);
        }
    }

    /* Earth mound and grass blades of random height */
    sheet_rect(&sh, 0, 2, GRAVE_FOOT + 1, 30, GRAVE_H, EARTH, 0.3f);
    for (int x = 1; x < GRAVE_W - 1; x++) {
        int blade = 1 + (int)(noise01(x, 401) * 5);
        Rgb c = x % 3 == 0 ? GRASS_LIT : GRASS;
        sheet_rect(&sh, 0, x, GRAVE_H - 2 - blade, x + 1, GRAVE_H - 1, c, 0.2f);
    }
}

#define STATUE_W      32
#define STATUE_H      88
#define STATUE_PLINTH 72   /* plinth from this row down */
#define STATUE_CX     16.0f

/* Weeping angel on a plinth: wings folded behind, head bowed into its
 * hands, robe falling to the plinth. Light from the upper left.
 *
 *     \ (o) /     wings behind the head and shoulders
 *     |\/^\/|    hands to the face
 *     | /   \ |   robe widening to the hem
 *      /_____\
 *     [_______]   plinth
 */
void church_statue(Texture *tex) {
    Sheet sh = sheet_alloc(tex, STATUE_W, STATUE_H, 1);

    /* Wings: tall ovals either side, tips above the head, narrowing
     * toward the top; feather rows in shadow */
    for (int side = -1; side <= 1; side += 2) {
        float wx = STATUE_CX + side * 9.5f;
        for (int y = 4; y < 66; y++) {
            for (int x = 0; x < STATUE_W; x++) {
                float narrow = y < 30 ? 0.55f + (y - 4) * 0.017f : 1.0f;
                float dx = (x + 0.5f - wx) / (6.0f * narrow);
                float dy = (y + 0.5f - 36.0f) / 31.0f;
                if (dx * dx + dy * dy > 1.0f) {
                    continue;
                }
                float k = side < 0 ? 0.85f : 0.6f;
                if (y % 5 == 0 && y > 24) {
                    k -= 0.2f;
                }
                sheet_px(&sh, 0, x, y, scaled(STATUE, k), 0.15f);
            }
        }
    }

    /* Robe: widens from the shoulders to the hem, folds as darker lines */
    for (int y = 28; y < STATUE_PLINTH; y++) {
        float half = 5.0f + (y - 28) * 0.15f;
        for (int x = 0; x < STATUE_W; x++) {
            float u = (x + 0.5f - STATUE_CX) / half;
            if (u < -1.0f || u > 1.0f) {
                continue;
            }
            float k = 1.0f - (u + 1.0f) * 0.22f;
            if (x % 3 == 0 && y > 40) {
                k -= 0.15f;
            }
            sheet_px(&sh, 0, x, y, scaled(STATUE, k), 0.15f);
        }
    }

    /* Bowed head (set off from the wings by a shadow) and hands raised
     * to the face */
    sheet_ellipse(&sh, 0, STATUE_CX, 22.0f, 5.5f, 6.5f, scaled(STATUE, 0.35f), 0.1f);
    sheet_ellipse(&sh, 0, STATUE_CX, 22.0f, 4.0f, 5.0f, scaled(STATUE, 0.95f), 0.15f);
    sheet_ellipse(&sh, 0, STATUE_CX + 1.0f, 24.0f, 2.5f, 2.5f, scaled(STATUE, 0.55f), 0.1f);
    sheet_ellipse(&sh, 0, STATUE_CX, 27.0f, 3.5f, 2.0f, scaled(STATUE, 1.05f), 0.1f);

    /* Rain streaks down from the top */
    for (int x = 0; x < STATUE_W; x++) {
        if (noise01(x, 503) < 0.8f) {
            continue;
        }
        int len = 10 + (int)(noise01(x, 509) * 30);
        for (int y = 14; y < 14 + len && y < STATUE_PLINTH; y++) {
            int i = (y * tex->width + x) * 3;
            bool key = tex->pixels[i] == TEXTURE_KEY_R && tex->pixels[i + 1] == TEXTURE_KEY_G &&
                       tex->pixels[i + 2] == TEXTURE_KEY_B;
            if (!key) {
                sheet_px(&sh, 0, x, y, scaled(STATUE, 0.5f), 0.15f);
            }
        }
    }

    /* Plinth: cornice, panel, base course; moss at the foot */
    sheet_rect(&sh, 0, 5, STATUE_PLINTH + 3, 27, STATUE_H - 3, scaled(GRAVE, 0.85f), 0.2f);
    sheet_rect(&sh, 0, 8, STATUE_PLINTH + 6, 24, STATUE_H - 6, scaled(GRAVE, 0.65f), 0.2f);
    sheet_rect(&sh, 0, 3, STATUE_PLINTH, 29, STATUE_PLINTH + 3, scaled(GRAVE, 1.1f), 0.2f);
    sheet_rect(&sh, 0, 3, STATUE_H - 3, 29, STATUE_H, scaled(GRAVE, 0.75f), 0.2f);
    for (int x = 3; x < 29; x++) {
        if (noise01(x, 521) > 0.5f) {
            sheet_px(&sh, 0, x, STATUE_H - 4, MOSS, 0.2f);
        }
    }
}
