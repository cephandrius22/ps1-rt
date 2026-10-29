#include "settings.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SENSITIVITY_DEF 1.0f
#define VOLUME_DEF      0.8f
#define FOV_DEF         69.0f  /* ~1.2 rad, the original fixed view */
#define LINE_MAX_LEN    128

Settings settings_default(void) {
    return (Settings){.sensitivity = SENSITIVITY_DEF, .volume = VOLUME_DEF, .fov = FOV_DEF};
}

static float clamp_range(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

void settings_clamp(Settings *s) {
    s->sensitivity = clamp_range(s->sensitivity, SENSITIVITY_MIN, SENSITIVITY_MAX);
    s->volume = clamp_range(s->volume, VOLUME_MIN, VOLUME_MAX);
    s->fov = clamp_range(s->fov, FOV_MIN, FOV_MAX);
}

bool settings_load(Settings *s, const char *path) {
    *s = settings_default();
    FILE *f = fopen(path, "r");
    if (!f) {
        return false;
    }

    char line[LINE_MAX_LEN];
    while (fgets(line, sizeof(line), f)) {
        char key[32];
        float value;
        if (line[0] == '#' || sscanf(line, "%31s %f", key, &value) != 2 || !isfinite(value)) {
            continue;
        }
        if (strcmp(key, "sensitivity") == 0) {
            s->sensitivity = value;
        } else if (strcmp(key, "volume") == 0) {
            s->volume = value;
        } else if (strcmp(key, "fov") == 0) {
            s->fov = value;
        }
    }
    fclose(f);
    settings_clamp(s);
    return true;
}

bool settings_save(const Settings *s, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) {
        return false;
    }
    fprintf(f, "# Written by the options menu\nsensitivity %g\nvolume %g\nfov %g\n",
            s->sensitivity, s->volume, s->fov);
    bool ok = !ferror(f);
    fclose(f);
    return ok;
}
