#include "test_framework.h"
#include "editor_export.h"
#include "entity.h"
#include "proctex.h"
#include <stdlib.h>
#include <string.h>

#define FGD_TMP "build/tests/rt_test.fgd"
#define TGA_TMP "build/tests/rt_test.tga"
#define FGD_MAX 16384

static char fgd[FGD_MAX];

static void generate_fgd(void) {
    FILE *f = fopen(FGD_TMP, "w+");
    ASSERT_EQ_INT(editor_write_fgd(f), 0);
    rewind(f);
    size_t n = fread(fgd, 1, FGD_MAX - 1, f);
    fgd[n] = '\0';
    fclose(f);
    remove(FGD_TMP);
}

/* Every entity type and every declared key reaches the editor */
TEST(test_fgd_covers_entity_table) {
    generate_fgd();
    ASSERT(strstr(fgd, "= worldspawn") != NULL);
    ASSERT(strstr(fgd, "= info_player_start") != NULL);
    ASSERT(strstr(fgd, "= light") != NULL);

    for (int t = ENTITY_NONE + 1; t < ENTITY_TYPE_COUNT; t++) {
        const EntityDef *def = entity_def((EntityType)t);
        char cls[64];
        snprintf(cls, sizeof(cls), "= %s :", def->name);
        ASSERT(strstr(fgd, cls) != NULL);
        for (const EntityKey *k = def->keys; k && k->type != KEY_END; k++) {
            char key[64];
            snprintf(key, sizeof(key), "\t%s(", k->name);
            ASSERT(strstr(fgd, key) != NULL);
        }
    }
}

/* Lengths are shown in map units: decor width 0.8 m → 25.6 */
TEST(test_fgd_lengths_in_units) {
    generate_fgd();
    ASSERT(strstr(fgd, "w(float) : \"Sprite width (units)\" : \"25.6\"") != NULL);
}

/* Saved TGA loads back identically; color-keyed texels survive */
TEST(test_tga_roundtrip) {
    Texture a;
    proctex_generate(&a, PTEX_BARREL);
    ASSERT_EQ_INT(texture_save_tga(&a, TGA_TMP), 0);

    Texture b;
    ASSERT_EQ_INT(texture_load_tga(&b, TGA_TMP), 0);
    ASSERT_EQ_INT(b.width, a.width);
    ASSERT_EQ_INT(b.height, a.height);
    ASSERT(memcmp(a.pixels, b.pixels, (size_t)(a.width * a.height * 3)) == 0);
    ASSERT(!texture_opaque(&b, 0, 0)); /* top-left of the barrel is a hole */

    texture_free(&a);
    texture_free(&b);
    remove(TGA_TMP);
}

TEST(test_fgd_string_keys) {
    generate_fgd();
    ASSERT(strstr(fgd, "give(string) : \"Weapon: shotgun\" : \"shotgun\"") != NULL);
    ASSERT(strstr(fgd, "= weapon :") != NULL);
}

int main(void) {
    TEST_SUITE("editor_export");
    RUN_TEST(test_fgd_covers_entity_table);
    RUN_TEST(test_fgd_lengths_in_units);
    RUN_TEST(test_tga_roundtrip);
    RUN_TEST(test_fgd_string_keys);
    TEST_REPORT();
}
