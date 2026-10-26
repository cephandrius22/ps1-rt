#include "kv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KV_PATTERN_MAX 64

/* Find "key=" in line; returns pointer to the value or NULL */
static const char *kv_find(const char *line, const char *key) {
    char pattern[KV_PATTERN_MAX];
    snprintf(pattern, sizeof(pattern), "%s=", key);

    const char *found = strstr(line, pattern);
    if (!found) {
        return NULL;
    }
    return found + strlen(pattern);
}

float kv_float(const char *line, const char *key, float def) {
    const char *val = kv_find(line, key);
    if (!val) {
        return def;
    }
    return (float)atof(val);
}

int kv_int(const char *line, const char *key, int def) {
    const char *val = kv_find(line, key);
    if (!val) {
        return def;
    }
    return atoi(val);
}
