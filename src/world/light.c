#include "light.h"
#include "kv.h"
#include <stdlib.h>
#include <math.h>

#define DEG_TO_RAD ((float)MATH_PI / 180.0f)

void light_list_init(LightList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void light_list_free(LightList *list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

int light_add(LightList *list, Light light) {
    if (list->count >= list->capacity) {
        int new_cap = list->capacity == 0 ? 8 : list->capacity * 2;
        Light *new_items = realloc(list->items, (size_t)new_cap * sizeof(Light));
        if (!new_items) {
            return -1;
        }
        list->items = new_items;
        list->capacity = new_cap;
    }
    int idx = list->count++;
    list->items[idx] = light;
    return idx;
}

int light_add_temporary(LightList *list, Light light, float ttl) {
    light.ttl = ttl;
    light.ttl_total = ttl;
    return light_add(list, light);
}

/* Remove expired temporary lights: swap the last light into the hole */
static void retire_expired(LightList *list, float dt) {
    for (int i = 0; i < list->count;) {
        Light *l = &list->items[i];
        if (l->ttl_total <= 0) {
            i++;
            continue;
        }
        l->ttl -= dt;
        if (l->ttl > 0) {
            i++;
            continue;
        }
        list->items[i] = list->items[--list->count];
    }
}

void light_update(LightList *list, float time, float dt) {
    retire_expired(list, dt);

    for (int i = 0; i < list->count; i++) {
        Light *l = &list->items[i];
        if (l->swing_speed <= 0) {
            continue;
        }

        /* Pendulum: angle = max_angle * sin(speed * time)
         * Position swings in XZ plane around anchor point.
         * Use two slightly different frequencies for an elliptical path. */
        float angle_x = l->swing_angle * sinf(l->swing_speed * time);
        float angle_z = l->swing_angle * sinf(l->swing_speed * time * 0.7f + 1.0f);
        float sx = sinf(angle_x);
        float sz = sinf(angle_z);
        float cy = cosf(angle_x) * cosf(angle_z);

        l->position.x = l->anchor.x + l->cord_length * sx;
        l->position.z = l->anchor.z + l->cord_length * sz;
        l->position.y = l->anchor.y - l->cord_length * cy;
    }
}

float light_effective_intensity(const Light *light, float time) {
    if (light->off) {
        return 0;
    }
    float intensity = light->intensity;
    if (light->ttl_total > 0) {
        intensity *= light->ttl / light->ttl_total; /* linear fade-out */
    }
    if (light->flicker_speed <= 0 || light->flicker_amount <= 0) {
        return intensity;
    }

    /* Pseudo-random flicker using layered sine waves */
    float f = sinf(time * light->flicker_speed * 6.28f)
            * sinf(time * light->flicker_speed * 15.7f + 1.3f)
            * sinf(time * light->flicker_speed * 3.1f + 2.7f);
    f = f * 0.5f + 0.5f; /* 0 to 1 */
    return intensity * (1.0f - light->flicker_amount * f);
}

float light_attenuation(const Light *light, float distance) {
    if (distance >= light->radius) {
        return 0;
    }
    /* Smooth windowed falloff: inverse-square that fades to zero at radius */
    float d = distance / light->radius;
    float window = 1.0f - d * d;
    window = window * window; /* smooth fade at edge */
    float falloff = light->falloff > 0 ? light->falloff : LIGHT_FALLOFF_DEF;
    return window / (1.0f + distance * falloff);
}

#define BULB_RADIUS_BASE      0.08f /* meters */
#define BULB_RADIUS_PER_LIGHT 0.02f /* brighter bulbs look (and are) bigger */

float light_bulb_radius(const Light *light) {
    return BULB_RADIUS_BASE + BULB_RADIUS_PER_LIGHT * light->intensity;
}

int light_bulb_hit(const LightList *lights, Ray ray, float t_max, float *t) {
    int best = -1;
    for (int i = 0; i < lights->count; i++) {
        const Light *l = &lights->items[i];
        if (!l->breakable || l->off || l->ttl_total > 0) {
            continue;
        }

        /* Closest approach of the ray to the bulb's center */
        float along = vec3_dot(vec3_sub(l->position, ray.origin), ray.dir);
        if (along <= 0 || along >= t_max) {
            continue;
        }
        Vec3 closest = vec3_add(ray.origin, vec3_mul(ray.dir, along));
        float r = light_bulb_radius(l);
        if (vec3_length(vec3_sub(closest, l->position)) > r) {
            continue;
        }
        t_max = along;
        best = i;
    }
    if (best >= 0) {
        *t = t_max;
    }
    return best;
}

Light light_spot(Vec3 pos, Vec3 aim, Vec3 color, float intensity, float radius,
                 float half_angle) {
    return (Light){
        .kind = LIGHT_SPOT,
        .position = pos,
        .color = color,
        .intensity = intensity,
        .radius = radius,
        .aim = vec3_normalize(aim),
        .cone_cos = cosf(half_angle),
    };
}

float light_cone(const Light *light, Vec3 dir) {
    if (light->kind != LIGHT_SPOT) {
        return 1.0f;
    }
    float t = (vec3_dot(dir, light->aim) - light->cone_cos) / (1.0f - light->cone_cos);
    t = clampf(t, 0, 1);
    return t * t;
}

Light light_from_keys(Vec3 pos, Vec3 color, float intensity, float radius,
                           const char *args) {
    float cord = kv_float(args, "cord", 0);
    Light light = {
        .position = pos,
        .color = color,
        .intensity = intensity,
        .radius = radius,
        .cast_shadows = kv_int(args, "shadows", 0) != 0,
        .flicker_speed = kv_float(args, "flicker_speed", 0),
        .flicker_amount = kv_float(args, "flicker_amount", 0),
        .anchor = vec3_add(pos, vec3(0, cord, 0)),
        .cord_length = cord,
        .swing_speed = kv_float(args, "swing_speed", 0),
        .swing_angle = kv_float(args, "swing_angle", 0),
        .off = kv_int(args, "off", 0) != 0,
        .breakable = kv_int(args, "breakable", 0) != 0,
    };
    kv_string(args, "targetname", light.targetname, sizeof(light.targetname));

    float cone = kv_float(args, "cone", 0);
    if (cone > 0) {
        Vec3 aim = vec3(kv_float(args, "aim_x", 0), kv_float(args, "aim_y", -1),
                        kv_float(args, "aim_z", 0));
        Light spot = light_spot(pos, aim, color, intensity, radius, cone * DEG_TO_RAD);
        light.kind = spot.kind;
        light.aim = spot.aim;
        light.cone_cos = spot.cone_cos;
    }
    return light;
}
