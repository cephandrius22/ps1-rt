#include "light.h"
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
        if (!new_items) return -1;
        list->items = new_items;
        list->capacity = new_cap;
    }
    int idx = list->count++;
    list->items[idx] = light;
    return idx;
}

void light_update(LightList *list, float time) {
    for (int i = 0; i < list->count; i++) {
        PointLight *l = &list->items[i];
        if (l->swing_speed <= 0) continue;

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
    if (light->flicker_speed <= 0 || light->flicker_amount <= 0)
        return light->intensity;

    /* Pseudo-random flicker using layered sine waves */
    float f = sinf(time * light->flicker_speed * 6.28f)
            * sinf(time * light->flicker_speed * 15.7f + 1.3f)
            * sinf(time * light->flicker_speed * 3.1f + 2.7f);
    f = f * 0.5f + 0.5f; /* 0 to 1 */
    return light->intensity * (1.0f - light->flicker_amount * f);
}

float light_attenuation(const PointLight *light, float distance) {
    if (distance >= light->radius) return 0;
    /* Smooth windowed falloff: inverse-square that fades to zero at radius */
    float d = distance / light->radius;
    float window = 1.0f - d * d;
    window = window * window; /* smooth fade at edge */
    return window / (1.0f + distance * 0.5f);
}
