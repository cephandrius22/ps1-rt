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

/* An exit sealed in a box can't be walked to */
TEST(test_check_exit_unreachable) {
    const char *sealed = "spawn 0 0 0 0\nentity exit 3 0 3\n"
                         "box 1.5 0 1.5 4.5 2.9 1.7 c\nbox 1.5 0 4.3 4.5 2.9 4.5 c\n"
                         "box 1.5 0 1.7 1.7 2.9 4.3 c\nbox 4.3 0 1.7 4.5 2.9 4.3 c\n";
    ASSERT_EQ_INT(check_level(sealed), 1);
    ASSERT(strstr(report, "exit #0 can't be reached on foot from the spawn") != NULL);
}

/* Wiring: a target naming nothing, a named door nothing opens, a keyed
 * door whose keycard is missing */
TEST(test_check_wiring) {
    const char *ok = "spawn 0 0 0 0\nentity exit 3 0 3\n"
                     "brush trigger_once -1 0 1 1 2 2 c target=gate\n"
                     "brush func_door -1 0 -5 1 2.5 -4.8 c targetname=gate\n"
                     "brush func_door 2 0 -5 4 2.5 -4.8 c key=red\n"
                     "entity keycard 0 0 2 color=red\n";
    ASSERT_EQ_INT(check_level(ok), 0);

    const char *bad = "spawn 0 0 0 0\nentity exit 3 0 3\n"
                      "brush trigger_once -1 0 1 1 2 2 c target=gaet\n"
                      "brush func_door -1 0 -5 1 2.5 -4.8 c targetname=gate\n"
                      "brush func_door 2 0 -5 4 2.5 -4.8 c key=blue\n";
    ASSERT_EQ_INT(check_level(bad), 3);
    ASSERT(strstr(report, "trigger_once #1 targets 'gaet', but nothing has that name") != NULL);
    ASSERT(strstr(report, "func_door #2 is named 'gate', but nothing targets it") != NULL);
    ASSERT(strstr(report, "func_door #3 needs the blue keycard, but the level has none") != NULL);
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
    RUN_TEST(test_check_wiring);
    RUN_TEST(test_check_exit_unreachable);
    TEST_REPORT();
}
