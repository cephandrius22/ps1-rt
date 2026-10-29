#include "test_framework.h"
#include "bench.h"
#include <string.h>

#define BENCH_OUT "build/tests/bench_out.txt"
#define OUT_MAX   4096

static char text[OUT_MAX];

/* Run the bench into a scratch file; returns its status, text holds the table */
static int run(const char *const maps[], int count) {
    FILE *f = fopen(BENCH_OUT, "w+");
    int status = bench_run(maps, count, f);
    rewind(f);
    size_t n = fread(text, 1, OUT_MAX - 1, f);
    text[n] = '\0';
    fclose(f);
    remove(BENCH_OUT);
    return status;
}

/* A map gets a row with its numbers, and the table a mean */
TEST(test_bench_reports_map) {
    const char *maps[] = {"tests/test.map"};
    ASSERT_EQ_INT(run(maps, 1), 0);
    ASSERT(strstr(text, "tests/test.map") != NULL);
    ASSERT(strstr(text, "mean") != NULL);
}

TEST(test_bench_missing_map_fails) {
    const char *maps[] = {"build/tests/no_such.map"};
    ASSERT_EQ_INT(run(maps, 1), -1);
}

int main(void) {
    TEST_SUITE("bench");
    RUN_TEST(test_bench_reports_map);
    RUN_TEST(test_bench_missing_map_fails);
    TEST_REPORT();
}
