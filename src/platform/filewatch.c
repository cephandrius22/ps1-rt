#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L /* struct stat st_mtim on Linux */
#endif

#include "filewatch.h"
#include <stdio.h>
#include <sys/stat.h>

#define NS_PER_SEC 1000000000LL
#define MTIME_NONE (-1LL) /* file missing (e.g. mid-save rename) */

/* --- Platform: where struct stat keeps the modification time --- */

#if defined(__APPLE__)
static long long stat_mtime_ns(const struct stat *st) {
    return (long long)st->st_mtimespec.tv_sec * NS_PER_SEC + st->st_mtimespec.tv_nsec;
}
#elif defined(_WIN32)
/* MinGW's stat has whole seconds only. Enough: a change must hold for a
 * poll (0.25 s) before it's reported, and saves are seconds apart. */
static long long stat_mtime_ns(const struct stat *st) {
    return (long long)st->st_mtime * NS_PER_SEC;
}
#else
static long long stat_mtime_ns(const struct stat *st) {
    return (long long)st->st_mtim.tv_sec * NS_PER_SEC + st->st_mtim.tv_nsec;
}
#endif

/* --- Watch --- */

/* Modification time in nanoseconds, or MTIME_NONE */
static long long mtime_ns(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return MTIME_NONE;
    }
    return stat_mtime_ns(&st);
}

void filewatch_init(FileWatch *w, const char *path) {
    snprintf(w->path, sizeof(w->path), "%s", path);
    w->seen = mtime_ns(path);
    w->pending = w->seen;
}

FileStatus filewatch_poll(FileWatch *w) {
    long long now = mtime_ns(w->path);
    if (now == MTIME_NONE || now == w->seen) {
        w->pending = w->seen;
        return FILE_UNCHANGED;
    }

    /* New time: wait one poll to see it hold before reporting */
    if (now != w->pending) {
        w->pending = now;
        return FILE_UNCHANGED;
    }
    w->seen = now;
    return FILE_CHANGED;
}
