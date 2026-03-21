/*
 * bvh.h — Bounding Volume Hierarchy for accelerated ray-scene intersection.
 *
 * Builds a binary tree of axis-aligned bounding boxes using the Surface Area
 * Heuristic (SAH). Traversal is stack-based and iterative. Leaf nodes contain
 * up to 4 triangles.
 */
#ifndef BVH_H
#define BVH_H

#include "mesh.h"

typedef struct {
    Vec3 min, max;
} AABB;

typedef struct {
    AABB bbox;
    int left;      // index of left child, or -1 if leaf
    int right;     // index of right child
    int tri_start; // first triangle index (leaf only)
    int tri_count; // number of triangles (leaf only, 0 for internal)
} BVHNode;

typedef struct {
    BVHNode *nodes;
    int node_count;
    int capacity;
    Triangle *tris;  // sorted copy of triangles
    int tri_count;
} BVH;

void bvh_build(BVH *bvh, const Triangle *tris, int count);
void bvh_free(BVH *bvh);
HitRecord bvh_intersect(const BVH *bvh, Ray ray, float t_min, float t_max);
bool aabb_hit(AABB box, Ray ray, float t_min, float t_max);

#endif
