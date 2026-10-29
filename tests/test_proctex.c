#include "test_framework.h"
#include "proctex.h"
#include <stdlib.h>
#include <string.h>

TEST(test_proctex_find_concrete) {
    ASSERT_EQ_INT(proctex_find("concrete"), PTEX_CONCRETE);
}

TEST(test_proctex_find_all_names) {
    ASSERT_EQ_INT(proctex_find("concrete"), PTEX_CONCRETE);
    ASSERT_EQ_INT(proctex_find("brick"), PTEX_BRICK);
    ASSERT_EQ_INT(proctex_find("metal_panel"), PTEX_METAL_PANEL);
    ASSERT_EQ_INT(proctex_find("metal_grate"), PTEX_METAL_GRATE);
    ASSERT_EQ_INT(proctex_find("rust"), PTEX_RUST);
    ASSERT_EQ_INT(proctex_find("floor_tile"), PTEX_FLOOR_TILE);
    ASSERT_EQ_INT(proctex_find("pipe"), PTEX_PIPE);
}

TEST(test_proctex_find_none) {
    ASSERT_EQ_INT(proctex_find("none"), -1);
}

TEST(test_proctex_find_null) {
    ASSERT_EQ_INT(proctex_find(NULL), -1);
}

TEST(test_proctex_find_unknown) {
    ASSERT_EQ_INT(proctex_find("bogus"), -1);
}

TEST(test_proctex_generate_concrete) {
    Texture tex = {0};
    proctex_generate(&tex, PTEX_CONCRETE);
    ASSERT(tex.pixels != NULL);
    ASSERT_EQ_INT(tex.width, 32);
    ASSERT_EQ_INT(tex.height, 32);
    texture_free(&tex);
}

TEST(test_proctex_generate_brick) {
    Texture tex = {0};
    proctex_generate(&tex, PTEX_BRICK);
    ASSERT(tex.pixels != NULL);
    ASSERT_EQ_INT(tex.width, 64);
    ASSERT_EQ_INT(tex.height, 64);
    texture_free(&tex);
}

TEST(test_proctex_generate_all) {
    Texture textures[PTEX_COUNT];
    proctex_generate_all(textures);
    for (int i = 0; i < PTEX_COUNT; i++) {
        ASSERT(textures[i].pixels != NULL);
        ASSERT(textures[i].width > 0);
        ASSERT(textures[i].height > 0);
    }
    for (int i = 0; i < PTEX_COUNT; i++) {
        texture_free(&textures[i]);
    }
}

TEST(test_proctex_sample) {
    Texture tex = {0};
    proctex_generate(&tex, PTEX_CONCRETE);
    Vec3 col = texture_sample(&tex, 0.5f, 0.5f);
    ASSERT(col.x >= 0 && col.x <= 1);
    ASSERT(col.y >= 0 && col.y <= 1);
    ASSERT(col.z >= 0 && col.z <= 1);
    texture_free(&tex);
}

TEST(test_proctex_nonzero_content) {
    Texture tex = {0};
    proctex_generate(&tex, PTEX_METAL_PANEL);
    int nonzero = 0;
    for (int i = 0; i < tex.width * tex.height * 3; i++) {
        if (tex.pixels[i] != 0) {
            nonzero = 1;
            break;
        }
    }
    ASSERT(nonzero);
    texture_free(&tex);
}

/* Every texture's name finds it again (no gaps in the name table) */
TEST(test_proctex_names_round_trip) {
    for (int i = 0; i < PTEX_COUNT; i++) {
        const char *name = proctex_name((ProceduralTextureID)i);
        ASSERT(name != NULL);
        ASSERT_EQ_INT(proctex_find(name), i);
    }
}

TEST(test_proctex_church_sizes) {
    for (int i = PTEX_ASHLAR; i <= PTEX_ROSE_WINDOW; i++) {
        Texture tex = {0};
        proctex_generate(&tex, (ProceduralTextureID)i);
        bool slab = i == PTEX_TOMB_SLAB;
        ASSERT_EQ_INT(tex.width, slab ? 32 : 64);
        ASSERT_EQ_INT(tex.height, 64);
        texture_free(&tex);
    }
}

/* Mean channel difference between column a and column b */
static float column_diff(const Texture *t, int a, int b) {
    int sum = 0;
    for (int y = 0; y < t->height; y++) {
        for (int c = 0; c < 3; c++) {
            sum += abs(t->pixels[(y * t->width + a) * 3 + c] - t->pixels[(y * t->width + b) * 3 + c]);
        }
    }
    return (float)sum / (t->height * 3);
}

/* Transposed copy, so rows can be checked as columns */
static Texture transposed(const Texture *t) {
    Texture out = {.width = t->height, .height = t->width,
                   .pixels = malloc((size_t)(t->width * t->height * 3))};
    for (int y = 0; y < t->height; y++) {
        for (int x = 0; x < t->width; x++) {
            memcpy(&out.pixels[(x * out.width + y) * 3], &t->pixels[(y * t->width + x) * 3], 3);
        }
    }
    return out;
}

/* The wrap from the last column to the first is no harsher than the
 * harshest step inside the texture (a joint): no visible seam */
static bool wraps_cleanly(const Texture *t) {
    float inside = 0;
    for (int x = 0; x + 1 < t->width; x++) {
        float d = column_diff(t, x, x + 1);
        inside = d > inside ? d : inside;
    }
    return column_diff(t, t->width - 1, 0) <= inside * 1.05f;
}

TEST(test_proctex_church_tiles_seamlessly) {
    for (int i = PTEX_ASHLAR; i <= PTEX_STAINED_GLASS; i++) {
        if (i == PTEX_TOMB_SLAB) {
            continue; /* a single piece, not a tile */
        }
        Texture tex = {0};
        proctex_generate(&tex, (ProceduralTextureID)i);
        Texture rows = transposed(&tex);
        ASSERT(wraps_cleanly(&tex));
        ASSERT(wraps_cleanly(&rows));
        texture_free(&rows);
        texture_free(&tex);
    }
}

/* Church sprites: their sizes, and a color-keyed corner (holes) */
TEST(test_proctex_church_sprites) {
    struct { ProceduralTextureID id; int w, h; } sprites[] = {
        {PTEX_CANDELABRA, 24, 64}, {PTEX_GRAVESTONE, 32, 40}, {PTEX_STATUE, 32, 88},
    };
    for (size_t i = 0; i < sizeof(sprites) / sizeof(sprites[0]); i++) {
        Texture tex = {0};
        proctex_generate(&tex, sprites[i].id);
        ASSERT_EQ_INT(tex.width, sprites[i].w);
        ASSERT_EQ_INT(tex.height, sprites[i].h);
        ASSERT(!texture_opaque(&tex, 0.0f, 0.0f));
        texture_free(&tex);
    }
}

int main(void) {
    TEST_SUITE("proctex");
    RUN_TEST(test_proctex_find_concrete);
    RUN_TEST(test_proctex_find_all_names);
    RUN_TEST(test_proctex_find_none);
    RUN_TEST(test_proctex_find_null);
    RUN_TEST(test_proctex_find_unknown);
    RUN_TEST(test_proctex_generate_concrete);
    RUN_TEST(test_proctex_generate_brick);
    RUN_TEST(test_proctex_generate_all);
    RUN_TEST(test_proctex_sample);
    RUN_TEST(test_proctex_nonzero_content);
    RUN_TEST(test_proctex_names_round_trip);
    RUN_TEST(test_proctex_church_sizes);
    RUN_TEST(test_proctex_church_tiles_seamlessly);
    RUN_TEST(test_proctex_church_sprites);
    TEST_REPORT();
}
