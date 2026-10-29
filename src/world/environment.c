#include "environment.h"
#include <stdio.h>
#include <string.h>

#define AMBIENT_DEF       0.04f
#define SUN_INTENSITY_DEF 0.15f
#define FOG_START_DEF     8.0f
#define FOG_END_DEF       22.0f

Environment environment_default(void) {
    return (Environment){
        .ambient = AMBIENT_DEF,
        .sun_dir = vec3_normalize(vec3(0.5f, 1.0f, -0.3f)),
        .sun_color = vec3(1.0f, 0.95f, 0.7f),
        .sun_intensity = SUN_INTENSITY_DEF,
        .fog_color = vec3(0.04f, 0.03f, 0.02f),
        .fog_start = FOG_START_DEF,
        .fog_end = FOG_END_DEF,
        .sky_top = vec3(0.02f, 0.01f, 0.04f),
        .sky_horizon = vec3(0.08f, 0.06f, 0.03f),
    };
}

static bool set_ambient(Environment *env, const char *value) {
    float a;
    if (sscanf(value, "%f", &a) != 1) {
        return false;
    }
    env->ambient = a;
    return true;
}

static bool set_fog(Environment *env, const char *value) {
    float start, end, r, g, b;
    if (sscanf(value, "%f %f %f %f %f", &start, &end, &r, &g, &b) != 5 || end <= start) {
        return false;
    }
    env->fog_start = start;
    env->fog_end = end;
    env->fog_color = vec3(r, g, b);
    return true;
}

static bool set_sun(Environment *env, const char *value) {
    float dx, dy, dz, r, g, b, intensity;
    if (sscanf(value, "%f %f %f %f %f %f %f", &dx, &dy, &dz, &r, &g, &b, &intensity) != 7) {
        return false;
    }
    Vec3 dir = vec3(dx, dy, dz);
    if (vec3_length(dir) <= 0) {
        return false;
    }
    env->sun_dir = vec3_normalize(dir);
    env->sun_color = vec3(r, g, b);
    env->sun_intensity = intensity;
    return true;
}

static bool set_sky(Environment *env, const char *value) {
    float tr, tg, tb, hr, hg, hb;
    if (sscanf(value, "%f %f %f %f %f %f", &tr, &tg, &tb, &hr, &hg, &hb) != 6) {
        return false;
    }
    env->sky_top = vec3(tr, tg, tb);
    env->sky_horizon = vec3(hr, hg, hb);
    return true;
}

typedef struct {
    const char *key;
    bool (*set)(Environment *env, const char *value);
} EnvKey;

static const EnvKey env_keys[] = {
    {"ambient", set_ambient},
    {"fog", set_fog},
    {"sun", set_sun},
    {"sky", set_sky},
};

bool environment_set(Environment *env, const char *key, const char *value) {
    for (size_t i = 0; i < sizeof(env_keys) / sizeof(env_keys[0]); i++) {
        if (strcmp(env_keys[i].key, key) == 0) {
            return env_keys[i].set(env, value);
        }
    }
    return false;
}

float environment_fog(const Environment *env, float distance) {
    return clampf((distance - env->fog_start) / (env->fog_end - env->fog_start), 0, 1);
}

Vec3 environment_sky(const Environment *env, Vec3 dir) {
    float up = 0.5f * (dir.y + 1.0f);
    return vec3_lerp(env->sky_horizon, env->sky_top, up);
}
