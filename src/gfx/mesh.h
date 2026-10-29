/*
 * mesh.h — Triangle mesh storage, ray-triangle intersection, and OBJ loading.
 *
 * Uses Moller-Trumbore algorithm for ray-triangle intersection.
 * The OBJ loader supports v/vt/vn and triangulates quads via fan decomposition.
 */
#ifndef MESH_H
#define MESH_H

#include "math3d.h"
#include <stdbool.h>

typedef struct {
    Vec3 v0, v1, v2;
    Vec2 uv0, uv1, uv2;
    Vec3 normal;
    int material_id;
    int owner;  /* opaque tag set by whoever built the triangle; 0 = none */
} Triangle;

typedef struct {
    float t;
    float u, v;
    Vec3 point;
    Vec3 normal;
    Vec2 uv;
    int material_id;
    int owner;  /* copied from the hit triangle */
    const Triangle *tri; /* the triangle hit (NULL for sprites); valid until
                          * its BVH is rebuilt */
    bool hit;
} HitRecord;

/* Moller-Trumbore. Fills rec and returns true on a hit in [t_min, t_max];
 * rec is untouched on a miss. Hits both faces. */
bool ray_triangle_intersect(Ray ray, const Triangle *tri, float t_min, float t_max, HitRecord *rec);

typedef struct {
    Triangle *tris;
    int count;
    int capacity;
} Mesh;

/* Growable triangle array: init empty, add, free */
void mesh_init(Mesh *m);
void mesh_add_tri(Mesh *m, Triangle tri);
void mesh_free(Mesh *m);

/* Brute-force closest hit over every triangle (no BVH; tests/small meshes) */
HitRecord mesh_intersect(const Mesh *m, Ray ray, float t_min, float t_max);

/* Append an OBJ file's faces to m (polygons fan-triangulated).
 * Returns 0 on success, -1 if the file can't be read. */
int mesh_load_obj(Mesh *m, const char *filename);

#endif
