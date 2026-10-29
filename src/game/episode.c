#include "episode.h"
#include <stdio.h>
#include <string.h>

#define EPISODE_LINE_MAX 512
#define EPISODE_EXT      ".txt"

/* Copy text without leading/trailing spaces into out */
static void copy_trimmed(char *out, size_t size, const char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    size_t n = strcspn(text, "\r\n");
    while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t')) {
        n--;
    }
    snprintf(out, size, "%.*s", (int)n, text);
}

/* "map <path> [title words...]" */
static bool parse_map(Episode *ep, const char *rest) {
    if (ep->count >= EPISODE_MAPS_MAX) {
        fprintf(stderr, "episode: more than %d maps; the rest are ignored\n", EPISODE_MAPS_MAX);
        return false;
    }
    EpisodeMap *m = &ep->maps[ep->count];
    int used = 0;
    if (sscanf(rest, "%255s%n", m->path, &used) != 1) {
        return false;
    }
    copy_trimmed(m->title, sizeof(m->title), rest + used);
    ep->count++;
    return true;
}

bool episode_load(Episode *ep, const char *path) {
    *ep = (Episode){0};
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "episode: cannot open '%s'\n", path);
        return false;
    }

    char line[EPISODE_LINE_MAX];
    while (fgets(line, sizeof(line), f)) {
        const char *p = line + strspn(line, " \t");
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') {
            continue;
        }
        if (strncmp(p, "title ", 6) == 0) {
            copy_trimmed(ep->title, sizeof(ep->title), p + 6);
        } else if (strncmp(p, "map ", 4) == 0) {
            parse_map(ep, p + 4);
        } else {
            fprintf(stderr, "episode: unknown line: %.60s", p);
        }
    }
    fclose(f);

    if (ep->count == 0) {
        fprintf(stderr, "episode: '%s' lists no maps\n", path);
        return false;
    }
    return true;
}

Episode episode_single(const char *level_path) {
    Episode ep = {.count = 1};
    snprintf(ep.maps[0].path, sizeof(ep.maps[0].path), "%s", level_path);

    /* Title: the file name without folder or extension */
    const char *slash = strrchr(level_path, '/');
    const char *name = slash ? slash + 1 : level_path;
    size_t n = strcspn(name, ".");
    snprintf(ep.maps[0].title, sizeof(ep.maps[0].title), "%.*s", (int)n, name);
    return ep;
}

bool episode_is_file(const char *path) {
    size_t n = strlen(path);
    size_t e = strlen(EPISODE_EXT);
    return n >= e && strcmp(path + n - e, EPISODE_EXT) == 0;
}
