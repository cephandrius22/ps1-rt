/*
 * level.h — Text-based level file loader.
 *
 * Parses .level files containing material, geometry, entity, and light
 * definitions. Generates procedural textures, builds the scene, and
 * constructs the static BVH.
 */
#ifndef LEVEL_H
#define LEVEL_H

#include "scene.h"
#include "math3d.h"

typedef struct {
    Vec3 spawn_pos;
    float spawn_yaw; /* degrees */
} LevelInfo;

/* Load a .level file into the scene. Generates procedural textures,
 * builds materials, geometry, entities, lights, and static BVH.
 * Returns 0 on success, -1 on error. */
int level_load(Scene *scene, const char *filename, LevelInfo *info);

#endif
