#if !defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L /* struct stat st_mtim on Linux */
#endif

#include "filewatch.h"
#include <stdio.h>
#include <sys/stat.h>

#define NS_PER_SEC 1000000000LL
#define MTIME_NONE (-1LL) /* file missing (e.g. mid-save rename) */

/* Modification time in nanoseconds, or MTIME_NONE */
static long long mtime_ns(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return MTIME_NONE;
    }
#ifdef __APPLE__
    return (long long)st.st_mtimespec.tv_sec * NS_PER_SEC + st.st_mtimespec.tv_nsec;
#else
    return (long long)st.st_mtim.tv_sec * NS_PER_SEC + st.st_mtim.tv_nsec;
#endif
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
