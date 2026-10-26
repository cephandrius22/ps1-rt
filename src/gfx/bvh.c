#include "bvh.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>

static Vec3 vec3_min(Vec3 a, Vec3 b) {
    return (Vec3){fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)};
}

static Vec3 vec3_max(Vec3 a, Vec3 b) {
    return (Vec3){fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)};
}

static AABB tri_bounds(const Triangle *t) {
    Vec3 mn = vec3_min(vec3_min(t->v0, t->v1), t->v2);
    Vec3 mx = vec3_max(vec3_max(t->v0, t->v1), t->v2);
    // Pad thin AABBs
    float eps = 0.0001f;
    if (mx.x - mn.x < eps) { mn.x -= eps; mx.x += eps; }
    if (mx.y - mn.y < eps) { mn.y -= eps; mx.y += eps; }
    if (mx.z - mn.z < eps) { mn.z -= eps; mx.z += eps; }
    return (AABB){mn, mx};
}

static AABB aabb_union(AABB a, AABB b) {
    return (AABB){vec3_min(a.min, b.min), vec3_max(a.max, b.max)};
}

static float aabb_surface_area(AABB b) {
    Vec3 d = vec3_sub(b.max, b.min);
    return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
}

static Vec3 tri_centroid(const Triangle *t) {
    return vec3_mul(vec3_add(vec3_add(t->v0, t->v1), t->v2), 1.0f / 3.0f);
}

bool aabb_hit(AABB box, Ray ray, float t_min, float t_max) {
    for (int a = 0; a < 3; a++) {
        float origin, dir, bmin, bmax;
        if (a == 0) { origin = ray.origin.x; dir = ray.dir.x; bmin = box.min.x; bmax = box.max.x; }
        else if (a == 1) { origin = ray.origin.y; dir = ray.dir.y; bmin = box.min.y; bmax = box.max.y; }
        else { origin = ray.origin.z; dir = ray.dir.z; bmin = box.min.z; bmax = box.max.z; }

        float inv_d = 1.0f / dir;
        float t0 = (bmin - origin) * inv_d;
        float t1 = (bmax - origin) * inv_d;
        if (inv_d < 0.0f) { float tmp = t0; t0 = t1; t1 = tmp; }
        if (t0 > t_min) t_min = t0;
        if (t1 < t_max) t_max = t1;
        if (t_max <= t_min) return false;
    }
    return true;
}

static int alloc_node(BVH *bvh) {
    if (bvh->node_count >= bvh->capacity) {
        bvh->capacity *= 2;
        bvh->nodes = realloc(bvh->nodes, sizeof(BVHNode) * bvh->capacity);
    }
    int idx = bvh->node_count++;
    bvh->nodes[idx].left = -1;
    bvh->nodes[idx].right = -1;
    bvh->nodes[idx].tri_start = 0;
    bvh->nodes[idx].tri_count = 0;
    return idx;
}

static int build_recursive(BVH *bvh, int start, int count) {
    int node_idx = alloc_node(bvh);

    // Compute bounds
    AABB bounds = tri_bounds(&bvh->tris[start]);
    for (int i = 1; i < count; i++)
        bounds = aabb_union(bounds, tri_bounds(&bvh->tris[start + i]));
    bvh->nodes[node_idx].bbox = bounds;

    if (count <= 4) {
        // Leaf
        bvh->nodes[node_idx].tri_start = start;
        bvh->nodes[node_idx].tri_count = count;
        return node_idx;
    }

    // Find best split using SAH
    float best_cost = FLT_MAX;
    int best_axis = 0, best_split = -1;
    float parent_area = aabb_surface_area(bounds);

    for (int axis = 0; axis < 3; axis++) {
        // Sort by centroid on this axis
        for (int i = start; i < start + count - 1; i++) {
            for (int j = i + 1; j < start + count; j++) {
                float ci, cj;
                Vec3 centi = tri_centroid(&bvh->tris[i]);
                Vec3 centj = tri_centroid(&bvh->tris[j]);
                if (axis == 0) { ci = centi.x; cj = centj.x; }
                else if (axis == 1) { ci = centi.y; cj = centj.y; }
                else { ci = centi.z; cj = centj.z; }
                if (cj < ci) {
                    Triangle tmp = bvh->tris[i];
                    bvh->tris[i] = bvh->tris[j];
                    bvh->tris[j] = tmp;
                }
            }
        }

        // Evaluate splits
        for (int i = 1; i < count; i++) {
            AABB left_box = tri_bounds(&bvh->tris[start]);
            for (int j = 1; j < i; j++)
                left_box = aabb_union(left_box, tri_bounds(&bvh->tris[start + j]));

            AABB right_box = tri_bounds(&bvh->tris[start + i]);
            for (int j = i + 1; j < count; j++)
                right_box = aabb_union(right_box, tri_bounds(&bvh->tris[start + j]));

            float cost = 1.0f + (i * aabb_surface_area(left_box) +
                         (count - i) * aabb_surface_area(right_box)) / parent_area;

            if (cost < best_cost) {
                best_cost = cost;
                best_axis = axis;
                best_split = i;
            }
        }
    }

    // Re-sort by best axis
    for (int i = start; i < start + count - 1; i++) {
        for (int j = i + 1; j < start + count; j++) {
            float ci, cj;
            Vec3 centi = tri_centroid(&bvh->tris[i]);
            Vec3 centj = tri_centroid(&bvh->tris[j]);
            if (best_axis == 0) { ci = centi.x; cj = centj.x; }
            else if (best_axis == 1) { ci = centi.y; cj = centj.y; }
            else { ci = centi.z; cj = centj.z; }
            if (cj < ci) {
                Triangle tmp = bvh->tris[i];
                bvh->tris[i] = bvh->tris[j];
                bvh->tris[j] = tmp;
            }
        }
    }

    bvh->nodes[node_idx].left = build_recursive(bvh, start, best_split);
    bvh->nodes[node_idx].right = build_recursive(bvh, start + best_split, count - best_split);

    return node_idx;
}

void bvh_build(BVH *bvh, const Triangle *tris, int count) {
    bvh->tri_count = count;
    bvh->tris = malloc(sizeof(Triangle) * count);
    memcpy(bvh->tris, tris, sizeof(Triangle) * count);

    bvh->capacity = count * 2;
    bvh->nodes = malloc(sizeof(BVHNode) * bvh->capacity);
    bvh->node_count = 0;

    if (count > 0)
        build_recursive(bvh, 0, count);
}

void bvh_free(BVH *bvh) {
    free(bvh->nodes);
    free(bvh->tris);
    bvh->nodes = NULL;
    bvh->tris = NULL;
}

HitRecord bvh_intersect(const BVH *bvh, Ray ray, float t_min, float t_max) {
    HitRecord closest = {.hit = false, .t = t_max};

    if (bvh->node_count == 0) return closest;

    // Stack-based traversal
    int stack[64];
    int sp = 0;
    stack[sp++] = 0;

    while (sp > 0) {
        int idx = stack[--sp];
        const BVHNode *node = &bvh->nodes[idx];

        if (!aabb_hit(node->bbox, ray, t_min, closest.t))
            continue;

        if (node->tri_count > 0) {
            // Leaf
            for (int i = 0; i < node->tri_count; i++) {
                HitRecord rec;
                if (ray_triangle_intersect(ray, &bvh->tris[node->tri_start + i],
                                           t_min, closest.t, &rec)) {
                    closest = rec;
                }
            }
        } else {
            if (node->left >= 0) stack[sp++] = node->left;
            if (node->right >= 0) stack[sp++] = node->right;
        }
    }

    return closest;
}
