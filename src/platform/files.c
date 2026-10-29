#include "files.h"
#include <errno.h>

/* --- Platform: making a directory --- */

#ifdef _WIN32
#include <direct.h>

static int make_dir(const char *path) {
    return _mkdir(path);
}
#else
#include <sys/stat.h>

static int make_dir(const char *path) {
    return mkdir(path, 0755);
}
#endif

/* --- Public --- */

bool files_make_dir(const char *path) {
    return make_dir(path) == 0 || errno == EEXIST;
}
