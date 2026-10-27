/*
 * filewatch.h — Notice when a file on disk changes (level hot reload).
 *
 * Polls the file's modification time. A change is reported only once the
 * new time has held for one poll, so an editor that saves in several
 * writes isn't caught half-way:
 *
 *   poll: mtime A  A  B  B  B
 *   result:   -  -  -  ✓  -     (B seen twice in a row → changed)
 */
#ifndef FILEWATCH_H
#define FILEWATCH_H

#include <stdbool.h>

#define FILEWATCH_PATH_MAX 256

typedef struct {
    char path[FILEWATCH_PATH_MAX];
    long long seen;     /* mtime (ns) the caller last acted on */
    long long pending;  /* newer mtime waiting to be confirmed */
} FileWatch;

typedef enum {
    FILE_UNCHANGED,
    FILE_CHANGED,
} FileStatus;

/* Start watching path; its current state counts as already seen */
void filewatch_init(FileWatch *w, const char *path);

/* Call periodically (e.g. 4x per second) */
FileStatus filewatch_poll(FileWatch *w);

#endif
