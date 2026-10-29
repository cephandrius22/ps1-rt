#include "test_framework.h"
#include "episode.h"
#include "levelcheck.h"
#include "player.h"
#include <stdio.h>
#include <string.h>

#define EPISODE_TMP "build/tests/episode_tmp.txt"

TEST(test_episode_load) {
    FILE *f = fopen(EPISODE_TMP, "w");
    fputs("# a comment\ntitle  Descent \n\nmap levels/a.map Pumping Station\n"
          "map levels/b.level\n", f);
    fclose(f);

    Episode ep;
    ASSERT(episode_load(&ep, EPISODE_TMP));
    remove(EPISODE_TMP);
    ASSERT(strcmp(ep.title, "Descent") == 0);
    ASSERT_EQ_INT(ep.count, 2);
    ASSERT(strcmp(ep.maps[0].path, "levels/a.map") == 0);
    ASSERT(strcmp(ep.maps[0].title, "Pumping Station") == 0);
    ASSERT(strcmp(ep.maps[1].title, "") == 0);
}

TEST(test_episode_empty_or_missing) {
    FILE *f = fopen(EPISODE_TMP, "w");
    fputs("title Nothing\n", f);
    fclose(f);
    Episode ep;
    ASSERT(!episode_load(&ep, EPISODE_TMP));
    remove(EPISODE_TMP);
    ASSERT(!episode_load(&ep, "build/tests/no_such_episode.txt"));
}

TEST(test_episode_single) {
    Episode ep = episode_single("levels/arena.map");
    ASSERT_EQ_INT(ep.count, 1);
    ASSERT(strcmp(ep.maps[0].title, "arena") == 0);
    ASSERT(episode_is_file("levels/episode1.txt"));
    ASSERT(!episode_is_file("levels/arena.map"));
}

/* Every map of the shipped episode loads and passes the level checker
 * (spawn, exit reachable on foot, wiring, budgets) */
TEST(test_episode_shipped_maps_clean) {
    Episode ep;
    ASSERT(episode_load(&ep, "levels/episode1.txt"));
    BodySize player = {PLAYER_RADIUS, PLAYER_HEIGHT};
    for (int i = 0; i < ep.count; i++) {
        Scene s;
        scene_init(&s);
        LevelInfo info;
        ASSERT_EQ_INT(level_load(&s, ep.maps[i].path, &info), 0);
        FILE *out = fopen("build/tests/episode_check.txt", "w");
        int warnings = level_check(&s, &info, player, out);
        fclose(out);
        if (warnings != 0) {
            printf("\n    %s: %d checker warnings\n", ep.maps[i].path, warnings);
        }
        ASSERT_EQ_INT(warnings, 0);
        scene_free(&s);
    }
    remove("build/tests/episode_check.txt");
}

int main(void) {
    TEST_SUITE("episode");
    RUN_TEST(test_episode_load);
    RUN_TEST(test_episode_empty_or_missing);
    RUN_TEST(test_episode_single);
    RUN_TEST(test_episode_shipped_maps_clean);
    TEST_REPORT();
}
