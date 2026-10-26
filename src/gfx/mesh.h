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
} Triangle;

typedef struct {
    float t;
    float u, v;
    Vec3 point;
    Vec3 normal;
    Vec2 uv;
    int material_id;
    bool hit;
} HitRecord;

bool ray_triangle_intersect(Ray ray, const Triangle *tri, float t_min, float t_max, HitRecord *rec);

typedef struct {
    Triangle *tris;
    int count;
    int capacity;
} Mesh;

void mesh_init(Mesh *m);
void mesh_add_tri(Mesh *m, Triangle tri);
void mesh_free(Mesh *m);
HitRecord mesh_intersect(const Mesh *m, Ray ray, float t_min, float t_max);
int mesh_load_obj(Mesh *m, const char *filename);

#endif
