#include "nav.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FLOOR_MIN_NORMAL  0.5f   /* steeper surfaces are walls (as movement.c) */
#define SCAN_LIFT         1.0f   /* column scans start this far above the world */
#define SCAN_EPS          1e-3f
#define SCAN_MAX_HITS     32     /* surfaces looked at per column */
#define HEADROOM_START    0.002f /* headroom probe starts just above the floor, below
                                    * any thin slab lying on it (a 2 cm pool) */
#define PROBE_LOW         0.1f   /* link probe above the higher floor of the pair */
#define PROBE_HIGH        (NAV_HEADROOM - 0.1f)
#define HOLE_DEPTH        0.2f   /* ground between nodes may dip this far */
#define CLEARANCE         0.4f   /* a wall this close makes a node tight */
#define TIGHT_COST        3.0f   /* cost multiplier for entering a tight node */
#define LOOKAHEAD         8      /* nodes one waypoint may skip along a straight run */
#define MAX_EXPAND        16384  /* nodes one search may close: bounds worst cases */
#define DIAGONAL          1.41421356f

#define ORTHO_DIRS 4
#define DIRS       8

/*  Directions; diagonals come after the four straight ones:
 *
 *       z-
 *    7  3  5         d:  0 +x   1 -x   2 +z   3 -z
 *  1  · 0  x+            4 +x+z 5 +x-z 6 -x+z 7 -x-z
 *    6  2  4
 *       z+
 */
static const int dir_x[DIRS] = {1, -1, 0, 0, 1, 1, -1, -1};
static const int dir_z[DIRS] = {0, 0, 1, -1, 1, -1, 1, -1};
static const int dir_opposite[DIRS] = {1, 0, 3, 2, 7, 6, 5, 4};

/* The two straight directions a diagonal passes between */
static const int diag_side_x[DIRS] = {-1, -1, -1, -1, 0, 0, 1, 1};
static const int diag_side_z[DIRS] = {-1, -1, -1, -1, 2, 3, 2, 3};

/* --- Node addressing --- */

static int node_index(const NavGrid *nav, int cx, int cz, int layer) {
    return (cz * nav->nx + cx) * NAV_LAYERS + layer;
}

static int node_cx(const NavGrid *nav, int node) {
    return (node / NAV_LAYERS) % nav->nx;
}

static int node_cz(const NavGrid *nav, int node) {
    return (node / NAV_LAYERS) / nav->nx;
}

static bool in_grid(const NavGrid *nav, int cx, int cz) {
    return cx >= 0 && cz >= 0 && cx < nav->nx && cz < nav->nz;
}

static bool has_floor(const NavGrid *nav, int node) {
    return !isnan(nav->height[node]);
}

Vec3 nav_node_pos(const NavGrid *nav, int node) {
    float x = nav->min_x + (node_cx(nav, node) + 0.5f) * NAV_CELL;
    float z = nav->min_z + (node_cz(nav, node) + 0.5f) * NAV_CELL;
    return vec3(x, nav->height[node], z);
}

/* The floor in the next column along d that a body on `node` would step
 * onto: the one closest in height, within NAV_STEP */
static int neighbor(const NavGrid *nav, int node, int d) {
    int cx = node_cx(nav, node) + dir_x[d];
    int cz = node_cz(nav, node) + dir_z[d];
    if (!in_grid(nav, cx, cz)) {
        return NAV_NONE;
    }

    float h = nav->height[node];
    int best = NAV_NONE;
    float best_dh = NAV_STEP;
    for (int l = 0; l < NAV_LAYERS; l++) {
        int n = node_index(nav, cx, cz, l);
        if (!has_floor(nav, n)) {
            continue;
        }
        float dh = fabsf(nav->height[n] - h);
        if (dh <= best_dh) {
            best = n;
            best_dh = dh;
        }
    }
    return best;
}

static bool linked(const NavGrid *nav, int node, int d) {
    return (nav->links[node] >> d) & 1;
}

/* --- Build --- */

static bool hits(const BVH *world, Vec3 from, Vec3 dir, float len) {
    Ray ray = {from, dir};
    return bvh_occluded(world, ray, SCAN_EPS, len);
}

/* Walk one column top-down, keeping up-facing surfaces with headroom.
 * Layers end up ordered highest first. */
static void scan_column(NavGrid *nav, const BVH *world, int cx, int cz, float top) {
    float x = nav->min_x + (cx + 0.5f) * NAV_CELL;
    float z = nav->min_z + (cz + 0.5f) * NAV_CELL;
    Ray down = {vec3(x, top, z), vec3(0, -1, 0)};
    float t_min = 0.0f;
    int layer = 0;

    for (int i = 0; i < SCAN_MAX_HITS && layer < NAV_LAYERS; i++) {
        HitRecord rec = bvh_intersect(world, down, t_min, INFINITY);
        if (!rec.hit) {
            return;
        }
        t_min = rec.t + SCAN_EPS;

        if (rec.normal.y <= FLOOR_MIN_NORMAL) {
            continue;
        }
        Vec3 above = vec3(x, rec.point.y + HEADROOM_START, z);
        if (hits(world, above, vec3(0, 1, 0), NAV_HEADROOM - HEADROOM_START)) {
            continue;
        }
        nav->height[node_index(nav, cx, cz, layer)] = rec.point.y;
        layer++;
    }
}

/* Nothing solid between two floors at foot (above the step) or head
 * height, and ground under the midpoint (no pit in between) */
static bool walkable(const NavGrid *nav, const BVH *world, int a, int b) {
    Vec3 pa = nav_node_pos(nav, a);
    Vec3 pb = nav_node_pos(nav, b);
    float high = fmaxf(pa.y, pb.y);
    float low = fminf(pa.y, pb.y);

    Vec3 span = vec3(pb.x - pa.x, 0, pb.z - pa.z);
    float len = vec3_length(span);
    Vec3 dir = vec3_mul(span, 1.0f / len);
    if (hits(world, vec3(pa.x, high + PROBE_LOW, pa.z), dir, len) ||
        hits(world, vec3(pa.x, high + PROBE_HIGH, pa.z), dir, len)) {
        return false;
    }

    Ray down = {vec3((pa.x + pb.x) * 0.5f, high + PROBE_LOW, (pa.z + pb.z) * 0.5f),
                vec3(0, -1, 0)};
    HitRecord rec = bvh_intersect(world, down, 0.0f, PROBE_LOW + (high - low) + HOLE_DEPTH);
    return rec.hit && rec.normal.y > FLOOR_MIN_NORMAL;
}

static void link_straight(NavGrid *nav, const BVH *world, int node) {
    for (int d = 0; d < ORTHO_DIRS; d++) {
        int n = neighbor(nav, node, d);
        if (n != NAV_NONE && walkable(nav, world, node, n)) {
            nav->links[node] |= (uint8_t)(1 << d);
        }
    }
}

/* Diagonal = both L-shaped detours exist and arrive at the same floor:
 *
 *    S1 ── B        A→S1→B and A→S2→B both walkable,
 *    │     │        so A→B can't clip the corner of a wall
 *    A ── S2
 */
static bool corner_clear(const NavGrid *nav, int a, int b, int d_first, int d_then) {
    if (!linked(nav, a, d_first)) {
        return false;
    }
    int side = neighbor(nav, a, d_first);
    return linked(nav, side, d_then) && neighbor(nav, side, d_then) == b;
}

static void link_diagonal(NavGrid *nav, int node) {
    for (int d = ORTHO_DIRS; d < DIRS; d++) {
        int n = neighbor(nav, node, d);
        if (n == NAV_NONE) {
            continue;
        }
        int sx = diag_side_x[d];
        int sz = diag_side_z[d];
        if (corner_clear(nav, node, n, sx, sz) && corner_clear(nav, node, n, sz, sx)) {
            nav->links[node] |= (uint8_t)(1 << d);
        }
    }
}

/* The floor a body falls to stepping off `node` in direction d: the
 * highest one more than a step lower, no deeper than NAV_DROP_MAX */
static int drop_target(const NavGrid *nav, int node, int d) {
    int cx = node_cx(nav, node) + dir_x[d];
    int cz = node_cz(nav, node) + dir_z[d];
    if (!in_grid(nav, cx, cz)) {
        return NAV_NONE;
    }
    float h = nav->height[node];
    for (int l = 0; l < NAV_LAYERS; l++) {   /* layers are highest first */
        int n = node_index(nav, cx, cz, l);
        if (!has_floor(nav, n)) {
            continue;
        }
        float below = h - nav->height[n];
        if (below > NAV_STEP && below <= NAV_DROP_MAX) {
            return n;
        }
    }
    return NAV_NONE;
}

#define DROP_LAND_SLACK 0.05f /* the fall must meet nothing but the floor */

/* Walk off the edge at the top floor's height, then fall straight onto
 * the lower floor: no wall in the way, no lip to land on first */
static bool droppable(const NavGrid *nav, const BVH *world, int top, int bottom) {
    Vec3 pa = nav_node_pos(nav, top);
    Vec3 pb = nav_node_pos(nav, bottom);
    Vec3 span = vec3(pb.x - pa.x, 0, pb.z - pa.z);
    float len = vec3_length(span);
    Vec3 dir = vec3_mul(span, 1.0f / len);
    if (hits(world, vec3(pa.x, pa.y + PROBE_LOW, pa.z), dir, len) ||
        hits(world, vec3(pa.x, pa.y + PROBE_HIGH, pa.z), dir, len)) {
        return false;
    }

    float fall = pa.y + PROBE_LOW - pb.y;
    Ray down = {vec3(pb.x, pa.y + PROBE_LOW, pb.z), vec3(0, -1, 0)};
    HitRecord rec = bvh_intersect(world, down, 0.0f, fall + DROP_LAND_SLACK);
    return rec.hit && rec.t >= fall - DROP_LAND_SLACK;
}

/* Drops only where the floor doesn't simply continue that way */
static void link_drops(NavGrid *nav, const BVH *world, int node) {
    for (int d = 0; d < ORTHO_DIRS; d++) {
        if (linked(nav, node, d)) {
            continue;
        }
        int below = drop_target(nav, node, d);
        if (below != NAV_NONE && droppable(nav, world, node, below)) {
            nav->drops[node] |= (uint8_t)(1 << d);
        }
    }
}

static bool near_wall(const NavGrid *nav, const BVH *world, int node) {
    Vec3 p = nav_node_pos(nav, node);
    Vec3 probe = vec3(p.x, p.y + PROBE_LOW + NAV_STEP, p.z);
    for (int d = 0; d < ORTHO_DIRS; d++) {
        if (hits(world, probe, vec3((float)dir_x[d], 0, (float)dir_z[d]), CLEARANCE)) {
            return true;
        }
    }
    return false;
}

static bool alloc_grid(NavGrid *nav) {
    int n = nav->node_count;
    nav->heap_cap = DIRS * (n < MAX_EXPAND ? n : MAX_EXPAND) + 1;

    nav->height = malloc(sizeof(float) * n);
    nav->links = calloc(n, 1);
    nav->tight = calloc(n, 1);
    nav->drops = calloc(n, 1);
    nav->closed = calloc(n, 1);
    nav->locked = calloc(n, 1);
    nav->seen = calloc(n, sizeof(uint32_t));
    nav->done = calloc(n, sizeof(uint32_t));
    nav->cost = malloc(sizeof(float) * n);
    nav->parent = malloc(sizeof(int) * n);
    nav->heap = malloc(sizeof(NavHeapItem) * nav->heap_cap);
    nav->flood_seen = calloc(n, sizeof(uint32_t));

    return nav->height && nav->links && nav->tight && nav->drops && nav->closed && nav->locked && nav->seen &&
           nav->done && nav->cost && nav->parent && nav->heap && nav->flood_seen;
}

void nav_build(NavGrid *nav, const BVH *world) {
    nav_free(nav);
    if (world->node_count == 0) {
        return;
    }

    /* Grid covers the world's bounds on X/Z */
    AABB box = world->nodes[0].bbox;
    nav->min_x = box.min.x;
    nav->min_z = box.min.z;
    nav->nx = (int)ceilf((box.max.x - box.min.x) / NAV_CELL) + 1;
    nav->nz = (int)ceilf((box.max.z - box.min.z) / NAV_CELL) + 1;
    nav->node_count = nav->nx * nav->nz * NAV_LAYERS;
    if (!alloc_grid(nav)) {
        nav_free(nav);
        return;
    }
    for (int i = 0; i < nav->node_count; i++) {
        nav->height[i] = NAN;
    }

    /* Floors, then straight links, then diagonals (which need straight) */
    float top = box.max.y + SCAN_LIFT;
    for (int cz = 0; cz < nav->nz; cz++) {
        for (int cx = 0; cx < nav->nx; cx++) {
            scan_column(nav, world, cx, cz, top);
        }
    }
    for (int i = 0; i < nav->node_count; i++) {
        if (has_floor(nav, i)) {
            link_straight(nav, world, i);
            nav->tight[i] = near_wall(nav, world, i);
        }
    }
    for (int i = 0; i < nav->node_count; i++) {
        if (has_floor(nav, i)) {
            link_drops(nav, world, i);
        }
    }
    for (int i = 0; i < nav->node_count; i++) {
        if (has_floor(nav, i)) {
            link_diagonal(nav, i);
        }
    }
}

void nav_free(NavGrid *nav) {
    free(nav->height);
    free(nav->links);
    free(nav->tight);
    free(nav->drops);
    free(nav->closed);
    free(nav->locked);
    free(nav->seen);
    free(nav->done);
    free(nav->cost);
    free(nav->parent);
    free(nav->heap);
    free(nav->flood_seen);
    memset(nav, 0, sizeof(*nav));
}

int nav_floor_count(const NavGrid *nav) {
    int count = 0;
    for (int i = 0; i < nav->node_count; i++) {
        count += has_floor(nav, i);
    }
    return count;
}

/* --- Lookup --- */

/* Highest floor in a column at most NAV_STEP above the feet */
static int floor_in_column(const NavGrid *nav, int cx, int cz, float feet_y) {
    if (!in_grid(nav, cx, cz)) {
        return NAV_NONE;
    }
    for (int l = 0; l < NAV_LAYERS; l++) {
        int n = node_index(nav, cx, cz, l);
        if (has_floor(nav, n) && nav->height[n] <= feet_y + NAV_STEP) {
            return n;
        }
    }
    return NAV_NONE;
}

int nav_node_at(const NavGrid *nav, Vec3 feet) {
    if (nav->node_count == 0) {
        return NAV_NONE;
    }
    int cx = (int)floorf((feet.x - nav->min_x) / NAV_CELL);
    int cz = (int)floorf((feet.z - nav->min_z) / NAV_CELL);
    int n = floor_in_column(nav, cx, cz, feet.y);
    if (n != NAV_NONE) {
        return n;
    }

    /* Own column has no floor (e.g. its center is inside a pillar the
     * body stands beside): take the closest neighbouring one */
    int best = NAV_NONE;
    float best_dist = INFINITY;
    for (int d = 0; d < DIRS; d++) {
        int m = floor_in_column(nav, cx + dir_x[d], cz + dir_z[d], feet.y);
        if (m == NAV_NONE) {
            continue;
        }
        Vec3 p = nav_node_pos(nav, m);
        float dist = vec3_length(vec3(p.x - feet.x, p.y - feet.y, p.z - feet.z));
        if (dist < best_dist) {
            best = m;
            best_dist = dist;
        }
    }
    return best;
}

/* --- Search --- */

static void heap_push(NavGrid *nav, float f, int node) {
    if (nav->heap_count >= nav->heap_cap) {
        return;
    }
    int i = nav->heap_count++;
    while (i > 0) {
        int up = (i - 1) / 2;
        if (nav->heap[up].f <= f) {
            break;
        }
        nav->heap[i] = nav->heap[up];
        i = up;
    }
    nav->heap[i] = (NavHeapItem){f, node};
}

static NavHeapItem heap_pop(NavGrid *nav) {
    NavHeapItem top = nav->heap[0];
    NavHeapItem last = nav->heap[--nav->heap_count];
    int i = 0;
    for (;;) {
        int child = 2 * i + 1;
        if (child >= nav->heap_count) {
            break;
        }
        if (child + 1 < nav->heap_count && nav->heap[child + 1].f < nav->heap[child].f) {
            child++;
        }
        if (last.f <= nav->heap[child].f) {
            break;
        }
        nav->heap[i] = nav->heap[child];
        i = child;
    }
    nav->heap[i] = last;
    return top;
}

/* Octile distance: exact on an empty 8-way grid, never overestimates */
static float estimate(const NavGrid *nav, int a, int b) {
    float dx = fabsf((float)(node_cx(nav, a) - node_cx(nav, b)));
    float dz = fabsf((float)(node_cz(nav, a) - node_cz(nav, b)));
    float straight = fabsf(dx - dz);
    float diagonal = fminf(dx, dz);
    return (straight + diagonal * DIAGONAL) * NAV_CELL;
}

static float step_cost(const NavGrid *nav, int from, int to, int d) {
    float len = (d < ORTHO_DIRS ? 1.0f : DIAGONAL) * NAV_CELL;
    return len + fabsf(nav->height[to] - nav->height[from]);
}

/* Relax neighbor m of node n, reached at cost g + the step */
static void relax(NavGrid *nav, int n, int m, int d, int goal, float g) {
    float c = g + step_cost(nav, n, m, d) * (nav->tight[m] ? TIGHT_COST : 1.0f);
    if (nav->seen[m] == nav->search && c >= nav->cost[m]) {
        return;
    }
    nav->seen[m] = nav->search;
    nav->cost[m] = c;
    nav->parent[m] = n;
    heap_push(nav, c + estimate(nav, m, goal), m);
}

/* Can a body on m get to n, which is in direction `back` from m: a walk
 * link, or a drop link that lands exactly on n */
static bool reaches(const NavGrid *nav, int m, int back, int n) {
    if (linked(nav, m, back) && neighbor(nav, m, back) == n) {
        return true;
    }
    bool drop = back < ORTHO_DIRS && ((nav->drops[m] >> back) & 1);
    return drop && drop_target(nav, m, back) == n;
}

/* Every floor in n's neighbouring column (direction d) that can reach n */
static void relax_column(NavGrid *nav, int n, int d, int start) {
    int cx = node_cx(nav, n) + dir_x[d];
    int cz = node_cz(nav, n) + dir_z[d];
    if (!in_grid(nav, cx, cz)) {
        return;
    }
    for (int l = 0; l < NAV_LAYERS; l++) {
        int m = node_index(nav, cx, cz, l);
        if (!has_floor(nav, m) || nav->locked[m] || nav->done[m] == nav->search) {
            continue;
        }
        if (reaches(nav, m, dir_opposite[d], n)) {
            relax(nav, n, m, d, start, nav->cost[n]);
        }
    }
}

/* A* run backwards, from the goal to the start: each node's parent then
 * points one step closer to the goal, so the start reads its path off
 * directly. Moves are checked in the walking direction (m → n). */
static bool search(NavGrid *nav, int start, int goal) {
    nav->search++;
    nav->heap_count = 0;
    nav->seen[goal] = nav->search;
    nav->cost[goal] = 0.0f;
    nav->parent[goal] = NAV_NONE;
    heap_push(nav, estimate(nav, goal, start), goal);

    for (int expanded = 0; nav->heap_count > 0 && expanded < MAX_EXPAND; expanded++) {
        int n = heap_pop(nav).node;
        if (nav->done[n] == nav->search) {
            continue;  /* stale duplicate: a cheaper copy was closed already */
        }
        nav->done[n] = nav->search;
        if (n == start) {
            return true;
        }

        for (int d = 0; d < DIRS; d++) {
            relax_column(nav, n, d, start);
        }
    }
    return false;
}

bool nav_next_waypoint(NavGrid *nav, Vec3 from, Vec3 to, Vec3 *waypoint) {
    int start = nav_node_at(nav, from);
    int goal = nav_node_at(nav, to);
    if (start == NAV_NONE || goal == NAV_NONE || nav->locked[goal]) {
        return false;
    }
    if (start == goal) {
        *waypoint = to;
        return true;
    }
    if (!search(nav, start, goal)) {
        return false;
    }

    /* Follow the path while it keeps going the same way: steer at the
     * corner, not at the next half-meter cell */
    int at = nav->parent[start];
    int step_x = node_cx(nav, at) - node_cx(nav, start);
    int step_z = node_cz(nav, at) - node_cz(nav, start);
    for (int i = 1; i < LOOKAHEAD; i++) {
        int next = nav->parent[at];
        if (next == NAV_NONE ||
            node_cx(nav, next) - node_cx(nav, at) != step_x ||
            node_cz(nav, next) - node_cz(nav, at) != step_z) {
            break;
        }
        at = next;
    }
    *waypoint = at == goal ? to : nav_node_pos(nav, at);
    return true;
}

/* --- Sound --- */

/* Dijkstra out to max_dist, through open links only; same scratch as
 * A* but stamps flood_seen so a later path search doesn't erase it */
void nav_flood(NavGrid *nav, Vec3 from, float max_dist) {
    nav->flood++;
    int source = nav_node_at(nav, from);
    if (source == NAV_NONE) {
        return;
    }

    nav->search++;
    nav->heap_count = 0;
    nav->seen[source] = nav->search;
    nav->cost[source] = 0.0f;
    heap_push(nav, 0.0f, source);

    while (nav->heap_count > 0) {
        NavHeapItem item = heap_pop(nav);
        int n = item.node;
        if (nav->done[n] == nav->search) {
            continue;
        }
        nav->done[n] = nav->search;
        nav->flood_seen[n] = nav->flood;

        for (int d = 0; d < DIRS; d++) {
            if (!linked(nav, n, d)) {
                continue;
            }
            int m = neighbor(nav, n, d);
            float c = item.f + step_cost(nav, n, m, d);
            if (nav->closed[m] || c > max_dist || nav->done[m] == nav->search) {
                continue;
            }
            if (nav->seen[m] == nav->search && c >= nav->cost[m]) {
                continue;
            }
            nav->seen[m] = nav->search;
            nav->cost[m] = c;
            heap_push(nav, c, m);
        }
    }
}

bool nav_heard(const NavGrid *nav, Vec3 feet) {
    int n = nav_node_at(nav, feet);
    return n != NAV_NONE && nav->flood_seen[n] == nav->flood;
}

/* --- Doors --- */

static void add_count(uint8_t *count, int delta) {
    int c = *count + delta;
    *count = (uint8_t)(c < 0 ? 0 : c);
}

void nav_set_door(NavGrid *nav, Vec3 lo, Vec3 hi, NavDoor kind, int delta) {
    float pad = NAV_CELL * 0.5f;
    for (int n = 0; n < nav->node_count; n++) {
        if (!has_floor(nav, n)) {
            continue;
        }
        Vec3 p = nav_node_pos(nav, n);
        bool inside = p.x >= lo.x - pad && p.x <= hi.x + pad &&
                      p.z >= lo.z - pad && p.z <= hi.z + pad &&
                      p.y >= lo.y - NAV_STEP && p.y <= hi.y;
        if (!inside) {
            continue;
        }
        add_count(&nav->closed[n], delta);
        if (kind == NAV_DOOR_LOCKED) {
            add_count(&nav->locked[n], delta);
        }
    }
}
