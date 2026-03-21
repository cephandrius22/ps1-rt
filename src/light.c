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
    /* Smooth inverse-square with range falloff */
    float d = distance / light->radius;
    float falloff = 1.0f - d * d;
    if (falloff < 0) falloff = 0;
    return falloff * falloff / (1.0f + distance * distance);
}
