#include "proctex.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
    if (v < 0) return 0;
    if (v > 255) return 255;
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
                if (rx * rx + ry * ry <= 2)
                    base += 0.07f;
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
                if (noise01(x * 3 + 70, y * 3 + 70) > 0.7f)
                    tex_set(tex, x, y, clamp8(v * 300), clamp8(v * 230), clamp8(v * 180));
                else
                    tex_set(tex, x, y, clamp8(v * 250), clamp8(v * 250), clamp8(v * 255));
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
            if (y % 8 == 0)
                base += 0.06f;

            /* Corroded, brownish */
            float rust_spot = noise01(x * 3 + 90, y * 3 + 90);
            if (rust_spot > 0.8f)
                tex_set(tex, x, y, clamp8(base * 300), clamp8(base * 220), clamp8(base * 160));
            else
                tex_set(tex, x, y, clamp8(base * 250), clamp8(base * 245), clamp8(base * 240));
        }
    }
}

/* --- Public API --- */

static const char *proctex_names[PTEX_COUNT] = {
    "concrete", "brick", "metal_panel", "metal_grate",
    "rust", "floor_tile", "pipe"
};

static void (*proctex_generators[PTEX_COUNT])(Texture *) = {
    gen_concrete, gen_brick, gen_metal_panel, gen_metal_grate,
    gen_rust, gen_floor_tile, gen_pipe
};

int proctex_find(const char *name) {
    if (!name || strcmp(name, "none") == 0)
        return -1;
    for (int i = 0; i < PTEX_COUNT; i++) {
        if (strcmp(name, proctex_names[i]) == 0)
            return i;
    }
    return -1;
}

void proctex_generate(Texture *tex, ProceduralTextureID id) {
    if (id >= 0 && id < PTEX_COUNT)
        proctex_generators[id](tex);
}

void proctex_generate_all(Texture *textures) {
    for (int i = 0; i < PTEX_COUNT; i++)
        proctex_generators[i](&textures[i]);
}
