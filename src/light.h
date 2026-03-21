/*
 * light.h — Real-time point light system with shadow rays.
 */
#ifndef LIGHT_H
#define LIGHT_H

#include "math3d.h"
#include <stdbool.h>

#define MAX_SHADOW_LIGHTS 4  /* max lights that cast shadow rays per pixel */

typedef struct {
    Vec3 position;
    Vec3 color;       /* RGB, each 0-1 */
    float intensity;
    float radius;     /* attenuation falloff range */
    bool cast_shadows;
    float flicker_speed;  /* 0 = no flicker */
    float flicker_amount; /* 0-1, how much intensity varies */
} PointLight;

typedef struct {
    PointLight *items;
    int count;
    int capacity;
} LightList;

void light_list_init(LightList *list);
void light_list_free(LightList *list);
int  light_add(LightList *list, PointLight light);

/* Get effective intensity for a light at a given time (applies flicker) */
float light_effective_intensity(const PointLight *light, float time);

/* Compute attenuation factor based on distance and light radius */
float light_attenuation(const PointLight *light, float distance);

#endif
