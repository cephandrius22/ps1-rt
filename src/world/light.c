#include "light.h"
#include "kv.h"
#include <stdlib.h>
#include <math.h>

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

int light_add(LightList *list, PointLight light) {
    if (list->count >= list->capacity) {
        int new_cap = list->capacity == 0 ? 8 : list->capacity * 2;
        PointLight *new_items = realloc(list->items, (size_t)new_cap * sizeof(PointLight));
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

int light_add_temporary(LightList *list, PointLight light, float ttl) {
    light.ttl = ttl;
    light.ttl_total = ttl;
    return light_add(list, light);
}

/* Remove expired temporary lights: swap the last light into the hole */
static void retire_expired(LightList *list, float dt) {
    for (int i = 0; i < list->count;) {
        PointLight *l = &list->items[i];
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
        PointLight *l = &list->items[i];
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

float light_effective_intensity(const PointLight *light, float time) {
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

float light_attenuation(const PointLight *light, float distance) {
    if (distance >= light->radius) {
        return 0;
    }
    /* Smooth windowed falloff: inverse-square that fades to zero at radius */
    float d = distance / light->radius;
    float window = 1.0f - d * d;
    window = window * window; /* smooth fade at edge */
    return window / (1.0f + distance * 0.5f);
}

PointLight light_from_keys(Vec3 pos, Vec3 color, float intensity, float radius,
                           const char *args) {
    float cord = kv_float(args, "cord", 0);
    return (PointLight){
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
    };
}
