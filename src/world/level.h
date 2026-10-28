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
    bool has_spawn;  /* false: the level never placed one (spawn is the origin) */
} LevelInfo;

/* Load a level into the scene: TrenchBroom .map (by extension, see
 * mapfile.h) or the .level text format. Generates procedural textures,
 * builds materials, geometry, entities, lights, and static BVH.
 * Returns 0 on success, -1 on error. */
int level_load(Scene *scene, const char *filename, LevelInfo *info);

struct MatLib;

/* Shared by both loaders: add a brush entity (door, trigger) covering
 * the world box [lo, hi]. tris are its faces in world space (may be
 * empty); drawn types keep them as the entity's model, others free them. */
void level_add_brush_entity(Scene *scene, EntityType type, Vec3 lo, Vec3 hi,
                            Mesh *tris, const char *args);

/* Shared last step of every loader: hand textures to the scene, jitter
 * world triangles, build the static BVH, free the world mesh */
void level_finish(Scene *scene, struct MatLib *mats, Mesh *world, const char *name);

#endif
