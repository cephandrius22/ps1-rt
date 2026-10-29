/*
 * files.h — Filesystem bits the C library doesn't cover portably.
 */
#ifndef FILES_H
#define FILES_H

#include <stdbool.h>

/* Create a directory (one level). True if it exists afterwards. */
bool files_make_dir(const char *path);

#endif
