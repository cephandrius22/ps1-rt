/*
 * mapfile.h — Load TrenchBroom / Quake .map levels.
 *
 * Map space is Quake's: Z up, 32 units = 1 meter. Points convert to the
 * engine's Y-up meters as (x, y, z) → (x, z, -y) / 32 (a pure rotation).
 *
 *   worldspawn, func_group, func_detail ── brushes ──> static triangles
 *   info_player_start  origin (feet), angle ──> spawn
 *   light              origin + intensity, radius (units), color, ... ──> Light
 *   <entity type name> origin, angle, keys ──> entity_spawn() (e.g. "ghoul")
 *   <brush type name>  brushes, angle, keys ──> level_add_brush_entity()
 *                      (func_door, func_button, trigger_*: bounds + model)
 *
 * Brushes are convex solids: each face's polygon is its plane clipped by
 * every other plane. Faces textured "skip", "clip" or "trigger" produce
 * no geometry (a trigger brush still has bounds).
 * Texture names may carry a folder ("rt/brick" → material "brick").
 * Both Standard and Valve 220 face formats are read; UVs use the texture's
 * pixel size so the game matches the editor (64 px texture = 2 m).
 */
#ifndef MAPFILE_H
#define MAPFILE_H

#include "level.h"

#define MAP_UNITS_PER_METER 32.0f

/* Load a .map into scene. Returns 0 on success, -1 on error. */
int mapfile_load(Scene *scene, const char *filename, LevelInfo *info);

/* Map-space point (Z up, units) → engine space (Y up, meters) */
static inline Vec3 map_to_engine(Vec3 p) {
    return vec3_mul(vec3(p.x, p.z, -p.y), 1.0f / MAP_UNITS_PER_METER);
}

/* Map "angle" (degrees, 0 = +X, counter-clockwise from above) → engine
 * yaw in degrees (0 = facing -Z, map +Y) */
static inline float map_angle_to_yaw(float angle) {
    return angle - 90.0f;
}

#endif
