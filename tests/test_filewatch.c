#include "test_framework.h"
#include "filewatch.h"
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

#define WATCHED "build/tests/filewatch_tmp.txt"

static void write_file(const char *text) {
    FILE *f = fopen(WATCHED, "w");
    fputs(text, f);
    fclose(f);
}

/* Force a distinct mtime (seconds since epoch) regardless of FS resolution */
static void set_mtime(long sec) {
    struct timeval t[2] = {{sec, 0}, {sec, 0}};
    utimes(WATCHED, t);
}

TEST(test_filewatch_reports_settled_change) {
    write_file("a");
    set_mtime(1000);
    FileWatch w;
    filewatch_init(&w, WATCHED);
    ASSERT(filewatch_poll(&w) == FILE_UNCHANGED);

    write_file("b");
    set_mtime(2000);
    ASSERT(filewatch_poll(&w) == FILE_UNCHANGED); /* first sighting: wait */
    ASSERT(filewatch_poll(&w) == FILE_CHANGED);   /* held: report once */
    ASSERT(filewatch_poll(&w) == FILE_UNCHANGED);
}

TEST(test_filewatch_waits_while_still_writing) {
    write_file("a");
    set_mtime(1000);
    FileWatch w;
    filewatch_init(&w, WATCHED);

    set_mtime(2000);
    ASSERT(filewatch_poll(&w) == FILE_UNCHANGED);
    set_mtime(3000); /* editor still writing */
    ASSERT(filewatch_poll(&w) == FILE_UNCHANGED);
    ASSERT(filewatch_poll(&w) == FILE_CHANGED);
}

TEST(test_filewatch_missing_file) {
    unlink(WATCHED);
    FileWatch w;
    filewatch_init(&w, WATCHED);
    ASSERT(filewatch_poll(&w) == FILE_UNCHANGED);
}

int main(void) {
    TEST_SUITE("filewatch");
    RUN_TEST(test_filewatch_reports_settled_change);
    RUN_TEST(test_filewatch_waits_while_still_writing);
    RUN_TEST(test_filewatch_missing_file);
    unlink(WATCHED);
    TEST_REPORT();
}
