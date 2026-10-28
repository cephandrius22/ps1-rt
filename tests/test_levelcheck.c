#include "test_framework.h"
#include "levelcheck.h"
#include <stdio.h>
#include <string.h>

#define LEVEL_TMP "build/tests/check_tmp.level"
#define OUT_MAX   4096

static const BodySize player = {0.3f, 1.6f};
static char report[OUT_MAX];

/* Room 10x3x10 around the origin plus extra lines; returns warning count
 * and leaves the printed report in `report` */
static int check_level(const char *extra) {
    FILE *f = fopen(LEVEL_TMP, "w");
    fprintf(f, "material c concrete 0.5 0.5 0.5\n"
               "room -5 0 -5 5 3 5 c c c\n%s", extra);
    fclose(f);

    Scene s;
    scene_init(&s);
    LevelInfo info;
    level_load(&s, LEVEL_TMP, &info);

    FILE *out = tmpfile();
    int warnings = level_check(&s, &info, player, out);
    rewind(out);
    size_t n = fread(report, 1, OUT_MAX - 1, out);
    report[n] = '\0';
    fclose(out);

    scene_free(&s);
    remove(LEVEL_TMP);
    return warnings;
}

TEST(test_check_clean_level) {
    ASSERT_EQ_INT(check_level("spawn 0 0 0 0\nentity exit 3 0 3\nentity ghoul 0 0 -3\n"), 0);
    ASSERT(strstr(report, "budget:") != NULL);
}

TEST(test_check_missing_spawn_and_exit) {
    ASSERT_EQ_INT(check_level(""), 2);
    ASSERT(strstr(report, "no player spawn") != NULL);
    ASSERT(strstr(report, "no exit pad") != NULL);
}

TEST(test_check_spawn_inside_box) {
    ASSERT_EQ_INT(check_level("spawn 0 0 0 0\nentity exit 3 0 3\nbox -1 0 -1 1 2 1 c\n"), 1);
    ASSERT(strstr(report, "player spawn is inside solid geometry") != NULL);
}

TEST(test_check_spawn_against_wall) {
    ASSERT_EQ_INT(check_level("spawn 4.9 0 0 0\nentity exit 3 0 3\n"), 1);
    ASSERT(strstr(report, "player spawn overlaps a wall") != NULL);
}

/* Enemies fall now: a raised one is fine; one over the void is not */
TEST(test_check_enemy_floor) {
    ASSERT_EQ_INT(check_level("spawn 0 0 0 0\nentity exit 3 0 3\nentity ghoul 0 1 -3\n"), 0);
    ASSERT_EQ_INT(check_level("spawn 0 0 0 0\nentity exit 3 0 3\nentity ghoul 0 1 20\n"), 1);
    ASSERT(strstr(report, "ghoul #1 has no floor below it") != NULL);
    ASSERT(strstr(report, "map (0 -640 32)") != NULL); /* position in TrenchBroom units */
}

TEST(test_check_over_budget) {
    char extra[4096] = "spawn 0 0 0 0\nentity exit 3 0 3\n";
    for (int i = 0; i <= BUDGET_SPRITES; i++) {
        strcat(extra, "entity decor 0 0 -3 solid=0\n");
    }
    ASSERT_EQ_INT(check_level(extra), 1);
    ASSERT(strstr(report, "over budget") != NULL);
}

/* Any enemy type gets the same checks (here: the spitter in a wall) */
TEST(test_check_spitter_in_wall) {
    ASSERT_EQ_INT(check_level("spawn 0 0 0 0\nentity exit 3 0 3\nentity spitter 4.9 0 -3\n"), 1);
    ASSERT(strstr(report, "spitter #1 overlaps a wall") != NULL);
}

int main(void) {
    TEST_SUITE("levelcheck");
    RUN_TEST(test_check_clean_level);
    RUN_TEST(test_check_missing_spawn_and_exit);
    RUN_TEST(test_check_spawn_inside_box);
    RUN_TEST(test_check_spawn_against_wall);
    RUN_TEST(test_check_enemy_floor);
    RUN_TEST(test_check_over_budget);
    RUN_TEST(test_check_spitter_in_wall);
    TEST_REPORT();
}
