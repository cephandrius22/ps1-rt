#include "bvh.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

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
    if (mx.x - mn.x < eps) {
        mn.x -= eps;
        mx.x += eps;
    }
    if (mx.y - mn.y < eps) {
        mn.y -= eps;
        mx.y += eps;
    }
    if (mx.z - mn.z < eps) {
        mn.z -= eps;
        mx.z += eps;
    }
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
        if (a == 0) {
            origin = ray.origin.x;
            dir = ray.dir.x;
            bmin = box.min.x;
            bmax = box.max.x;
        } else if (a == 1) {
            origin = ray.origin.y;
            dir = ray.dir.y;
            bmin = box.min.y;
            bmax = box.max.y;
        } else {
            origin = ray.origin.z;
            dir = ray.dir.z;
            bmin = box.min.z;
            bmax = box.max.z;
        }

        float inv_d = 1.0f / dir;
        float t0 = (bmin - origin) * inv_d;
        float t1 = (bmax - origin) * inv_d;
        if (inv_d < 0.0f) {
            float tmp = t0;
            t0 = t1;
            t1 = tmp;
        }
        if (t0 > t_min) {
            t_min = t0;
        }
        if (t1 < t_max) {
            t_max = t1;
        }
        if (t_max <= t_min) {
            return false;
        }
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

/* Swap two triangles, keeping track of where each came from */
static void swap_tris(BVH *bvh, int i, int j) {
    Triangle t = bvh->tris[i];
    bvh->tris[i] = bvh->tris[j];
    bvh->tris[j] = t;
    int o = bvh->order[i];
    bvh->order[i] = bvh->order[j];
    bvh->order[j] = o;
}

static int build_recursive(BVH *bvh, int start, int count) {
    int node_idx = alloc_node(bvh);

    // Compute bounds
    AABB bounds = tri_bounds(&bvh->tris[start]);
    for (int i = 1; i < count; i++) {
        bounds = aabb_union(bounds, tri_bounds(&bvh->tris[start + i]));
    }
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
                if (axis == 0) {
                    ci = centi.x;
                    cj = centj.x;
                } else if (axis == 1) {
                    ci = centi.y;
                    cj = centj.y;
                } else {
                    ci = centi.z;
                    cj = centj.z;
                }
                if (cj < ci) {
                    swap_tris(bvh, i, j);
                }
            }
        }

        // Evaluate splits
        for (int i = 1; i < count; i++) {
            AABB left_box = tri_bounds(&bvh->tris[start]);
            for (int j = 1; j < i; j++) {
                left_box = aabb_union(left_box, tri_bounds(&bvh->tris[start + j]));
            }

            AABB right_box = tri_bounds(&bvh->tris[start + i]);
            for (int j = i + 1; j < count; j++) {
                right_box = aabb_union(right_box, tri_bounds(&bvh->tris[start + j]));
            }

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
            if (best_axis == 0) {
                ci = centi.x;
                cj = centj.x;
            } else if (best_axis == 1) {
                ci = centi.y;
                cj = centj.y;
            } else {
                ci = centi.z;
                cj = centj.z;
            }
            if (cj < ci) {
                swap_tris(bvh, i, j);
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
    bvh->order = malloc(sizeof(int) * (count > 0 ? count : 1));
    for (int i = 0; i < count; i++) {
        bvh->order[i] = i;
    }

    bvh->capacity = count * 2;
    bvh->nodes = malloc(sizeof(BVHNode) * bvh->capacity);
    bvh->node_count = 0;

    if (count > 0) {
        build_recursive(bvh, 0, count);
    }
}

void bvh_free(BVH *bvh) {
    free(bvh->nodes);
    free(bvh->tris);
    free(bvh->order);
    bvh->nodes = NULL;
    bvh->tris = NULL;
    bvh->order = NULL;
}

bool bvh_same_layout(const BVH *bvh, const Triangle *tris, int count) {
    if (!bvh->order || count != bvh->tri_count) {
        return false;
    }
    for (int i = 0; i < count; i++) {
        if (tris[bvh->order[i]].owner != bvh->tris[i].owner) {
            return false;
        }
    }
    return true;
}

/* Children are allocated after their parent, so walking the nodes from
 * last to first visits every child before the node that holds it */
void bvh_refit(BVH *bvh, const Triangle *tris) {
    for (int i = 0; i < bvh->tri_count; i++) {
        bvh->tris[i] = tris[bvh->order[i]];
    }
    for (int n = bvh->node_count - 1; n >= 0; n--) {
        BVHNode *node = &bvh->nodes[n];
        if (node->tri_count > 0) {
            AABB box = tri_bounds(&bvh->tris[node->tri_start]);
            for (int i = 1; i < node->tri_count; i++) {
                box = aabb_union(box, tri_bounds(&bvh->tris[node->tri_start + i]));
            }
            node->bbox = box;
            continue;
        }
        AABB box = bvh->nodes[node->left].bbox;
        if (node->right >= 0) {
            box = aabb_union(box, bvh->nodes[node->right].bbox);
        }
        node->bbox = box;
    }
}

/* --- Traversal --- */

#define BVH_STACK_MAX 64

#define SLAB_DIR_MIN 1e-30f /* |dir| floor: keeps 1/dir finite */

/* A ray prepared for box tests: 1/dir once per ray, not per box */
typedef struct {
    Vec3 origin;
    Vec3 inv_dir;
} SlabRay;

/* 1/d, with d kept off zero: an infinite 1/d times a ray starting exactly
 * on a box face (0 * inf) would be NaN and lose the box */
static float safe_inv(float d) {
    return 1.0f / (fabsf(d) > SLAB_DIR_MIN ? d : copysignf(SLAB_DIR_MIN, d));
}

static SlabRay slab_ray(Ray ray) {
    return (SlabRay){ray.origin, vec3(safe_inv(ray.dir.x), safe_inv(ray.dir.y), safe_inv(ray.dir.z))};
}

/* Distance where the ray enters box within [t_min, t_max]; INFINITY on
 * a miss. Each axis clips the interval to its slab:
 *
 *     t_min ──[ x slab ]──[ y slab ]──> entry = latest start,
 *                                       exit  = earliest end
 */
static float box_entry(const AABB *b, const SlabRay *r, float t_min, float t_max) {
    float x0 = (b->min.x - r->origin.x) * r->inv_dir.x;
    float x1 = (b->max.x - r->origin.x) * r->inv_dir.x;
    float y0 = (b->min.y - r->origin.y) * r->inv_dir.y;
    float y1 = (b->max.y - r->origin.y) * r->inv_dir.y;
    float z0 = (b->min.z - r->origin.z) * r->inv_dir.z;
    float z1 = (b->max.z - r->origin.z) * r->inv_dir.z;
    t_min = fmaxf(t_min, fmaxf(fminf(x0, x1), fmaxf(fminf(y0, y1), fminf(z0, z1))));
    t_max = fminf(t_max, fminf(fmaxf(x0, x1), fminf(fmaxf(y0, y1), fmaxf(z0, z1))));
    return t_min <= t_max ? t_min : INFINITY;
}

typedef enum {
    HIT_CLOSEST,  /* camera rays, traces: the nearest triangle */
    HIT_ANY,      /* shadow rays: any triangle will do, stop at the first */
} HitMode;

typedef struct {
    int node;
    float entry;  /* where the ray enters the node's box */
} StackEntry;

/* Walk the tree: the nearer child first, and skip any box the ray
 * enters beyond the best hit so far. Returns the hit triangle's index
 * (best filled), or -1. */
static int traverse(const BVH *bvh, Ray ray, float t_min, float t_max, HitMode mode,
                    TriHit *best) {
    if (bvh->node_count == 0) {
        return -1;
    }
    SlabRay slab = slab_ray(ray);
    float root = box_entry(&bvh->nodes[0].bbox, &slab, t_min, t_max);
    if (root == INFINITY) {
        return -1;
    }

    StackEntry stack[BVH_STACK_MAX];
    int sp = 0;
    stack[sp++] = (StackEntry){0, root};
    int found = -1;
    best->t = t_max;

    while (sp > 0) {
        StackEntry top = stack[--sp];
        if (top.entry > best->t) {
            continue; /* a nearer hit was found since this was pushed */
        }
        const BVHNode *node = &bvh->nodes[top.node];

        if (node->tri_count > 0) {
            for (int i = node->tri_start; i < node->tri_start + node->tri_count; i++) {
                TriHit hit;
                if (!ray_triangle_hit(ray, &bvh->tris[i], t_min, best->t, &hit)) {
                    continue;
                }
                *best = hit;
                found = i;
                if (mode == HIT_ANY) {
                    return found;
                }
            }
            continue;
        }

        /* Push the farther child first so the nearer one pops next */
        float near_t = node->left >= 0 ? box_entry(&bvh->nodes[node->left].bbox, &slab, t_min, best->t) : INFINITY;
        float far_t = node->right >= 0 ? box_entry(&bvh->nodes[node->right].bbox, &slab, t_min, best->t) : INFINITY;
        int near = node->left;
        int far = node->right;
        if (far_t < near_t) {
            float t = near_t;
            near_t = far_t;
            far_t = t;
            near = node->right;
            far = node->left;
        }
        if (sp + 2 > BVH_STACK_MAX) {
            continue; /* deeper than any SAH tree we build */
        }
        if (far_t != INFINITY) {
            stack[sp++] = (StackEntry){far, far_t};
        }
        if (near_t != INFINITY) {
            stack[sp++] = (StackEntry){near, near_t};
        }
    }
    return found;
}

HitRecord bvh_intersect(const BVH *bvh, Ray ray, float t_min, float t_max) {
    TriHit best;
    int tri = traverse(bvh, ray, t_min, t_max, HIT_CLOSEST, &best);
    if (tri < 0) {
        return (HitRecord){.hit = false, .t = t_max};
    }
    return triangle_record(ray, &bvh->tris[tri], best);
}

bool bvh_occluded(const BVH *bvh, Ray ray, float t_min, float t_max) {
    TriHit hit;
    return traverse(bvh, ray, t_min, t_max, HIT_ANY, &hit) >= 0;
}
