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
            float base = 0.45f + noise01(x, y) * 0.12f - 0.06f;
            /* Darker stain patches */
            float stain = noise01(x / 4, y / 4) * 0.08f;
            float v = base - stain;
            uint8_t c = clamp8(v * 255.0f);
            /* Slight warm tint */
            tex_set(tex, x, y, c, clamp8(v * 245.0f), clamp8(v * 235.0f));
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

            /* Mortar lines */
            if (y_in_brick < mortar || x_in_brick < mortar) {
                float mv = 0.35f + noise01(x + 200, y + 200) * 0.06f;
                uint8_t mc = clamp8(mv * 255.0f);
                tex_set(tex, x, y, mc, mc, clamp8(mv * 240.0f));
                continue;
            }

            /* Brick face — per-brick color variation using brick grid coords */
            int brick_id_x = bx / brick_w;
            int brick_id_y = row;
            float variation = noise01(brick_id_x * 7 + 31, brick_id_y * 13 + 47) * 0.15f - 0.075f;
            float detail = noise01(x * 3 + 100, y * 3 + 100) * 0.04f - 0.02f;

            float r = 0.55f + variation + detail;
            float g = 0.32f + variation * 0.6f + detail;
            float b = 0.25f + variation * 0.4f + detail;
            tex_set(tex, x, y, clamp8(r * 255), clamp8(g * 255), clamp8(b * 255));
        }
    }
}

static void gen_metal_panel(Texture *tex) {
    tex_alloc(tex, 32, 32);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            float base = 0.35f + noise01(x, y) * 0.04f;

            /* Bevel edges */
            int dx = (x < 16) ? x : 31 - x;
            int dy = (y < 16) ? y : 31 - y;
            int edge = (dx < dy) ? dx : dy;

            if (edge == 0) {
                base -= 0.08f; /* dark outer edge */
            } else if (edge == 1) {
                base += 0.06f; /* highlight bevel */
            }

            /* Corner rivets (4 corners, 2px radius) */
            int corners[4][2] = {{4, 4}, {27, 4}, {4, 27}, {27, 27}};
            for (int c = 0; c < 4; c++) {
                int rx = x - corners[c][0];
                int ry = y - corners[c][1];
                if (rx * rx + ry * ry <= 2) {
                    base += 0.1f;
                }
            }

            uint8_t cv = clamp8(base * 255);
            tex_set(tex, x, y, cv, cv, clamp8(base * 260));
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
                /* Metal bar */
                float v = 0.45f + noise01(x, y) * 0.05f;
                if (on_h_bar && on_v_bar)
                    v += 0.05f; /* intersection brighter */
                tex_set(tex, x, y, clamp8(v * 255), clamp8(v * 255), clamp8(v * 260));
            } else {
                /* Hole — very dark */
                float v = 0.08f + noise01(x + 50, y + 50) * 0.04f;
                tex_set(tex, x, y, clamp8(v * 255), clamp8(v * 255), clamp8(v * 255));
            }
        }
    }
}

static void gen_rust(Texture *tex) {
    tex_alloc(tex, 32, 32);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            /* Vertical streak bias: stretch noise in Y */
            float n1 = noise01(x, y / 3);
            float n2 = noise01(x * 2 + 77, y + 33);
            float blend = n1 * 0.6f + n2 * 0.4f;

            float r = 0.50f + blend * 0.20f;
            float g = 0.28f + blend * 0.10f;
            float b = 0.12f + blend * 0.06f;

            /* Occasional darker pitting */
            if (noise01(x * 5, y * 5) > 0.85f) {
                r -= 0.1f;
                g -= 0.06f;
                b -= 0.03f;
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

            /* Grout lines */
            if (tx < grout || ty < grout) {
                float v = 0.25f + noise01(x + 300, y + 300) * 0.04f;
                uint8_t c = clamp8(v * 255);
                tex_set(tex, x, y, c, c, c);
                continue;
            }

            /* Two-tone checkerboard tiles */
            int tile_col = x / tile_size;
            int tile_row = y / tile_size;
            float base = ((tile_col + tile_row) % 2 == 0) ? 0.38f : 0.32f;
            base += noise01(x, y) * 0.03f;

            uint8_t c = clamp8(base * 255);
            tex_set(tex, x, y, c, c, clamp8(base * 250));
        }
    }
}

static void gen_pipe(Texture *tex) {
    tex_alloc(tex, 32, 32);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            /* Cylindrical highlight: brightest at center column */
            float cx = (float)x / 31.0f; /* 0 to 1 across width */
            float highlight = 1.0f - fabsf(cx - 0.5f) * 2.0f; /* peak at center */
            highlight = highlight * highlight; /* sharpen falloff */

            float base = 0.25f + highlight * 0.25f;
            base += noise01(x, y) * 0.03f;

            /* Band/rivet lines every 8 pixels */
            if (y % 8 == 0) {
                base += 0.08f;
            }

            /* Slight metallic blue tint */
            tex_set(tex, x, y, clamp8(base * 245), clamp8(base * 250), clamp8(base * 260));
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
