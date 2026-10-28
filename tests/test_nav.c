#include "test_framework.h"
#include "scene.h"
#include <math.h>

#define MAX_LEGS 64

static Scene s;
static Mesh m;

/* Axis-aligned solid box (outer faces), like the .level `box` directive */
static void add_box(Vec3 lo, Vec3 hi) {
    Vec3 v[8] = {
        vec3(lo.x, lo.y, lo.z), vec3(hi.x, lo.y, lo.z), vec3(hi.x, hi.y, lo.z), vec3(lo.x, hi.y, lo.z),
        vec3(lo.x, lo.y, hi.z), vec3(hi.x, lo.y, hi.z), vec3(hi.x, hi.y, hi.z), vec3(lo.x, hi.y, hi.z),
    };
    int faces[6][4] = {{4,5,6,7}, {1,0,3,2}, {0,4,7,3}, {5,1,2,6}, {7,6,2,3}, {0,1,5,4}};
    for (int f = 0; f < 6; f++) {
        Vec3 a = v[faces[f][0]], b = v[faces[f][1]], c = v[faces[f][2]], d = v[faces[f][3]];
        Vec3 n = vec3_normalize(vec3_cross(vec3_sub(b, a), vec3_sub(c, a)));
        mesh_add_tri(&m, (Triangle){.v0 = a, .v1 = b, .v2 = c, .normal = n});
        mesh_add_tri(&m, (Triangle){.v0 = a, .v1 = c, .v2 = d, .normal = n});
    }
}

/*  Top view (z grows down):
 *
 *   z -10  ┌stairs┐ 0.5/1.0/1.5 m steps, x -2..2
 *   z  -6  └──────┘
 *          ■ crate (1 m, x 8..9)        ▒ slab at 1.0–1.2 m
 *   z   0        o origin                 over x,z -15..-12
 *   z   4  ━━━━━━━━━ wall, x -6..6 (3 m tall)
 */
static void build_world(void) {
    scene_init(&s);
    mesh_init(&m);
    add_box(vec3(-20, -1, -20), vec3(20, 0, 20));
    add_box(vec3(-2, 0, -10), vec3(2, 0.5f, -6));
    add_box(vec3(-2, 0, -10), vec3(2, 1.0f, -7));
    add_box(vec3(-2, 0, -10), vec3(2, 1.5f, -8));
    add_box(vec3(-6, 0, 3.8f), vec3(6, 3, 4.2f));
    add_box(vec3(8, 0, -2), vec3(9, 1, -1));
    add_box(vec3(-15, 1.0f, -15), vec3(-12, 1.2f, -12));
    scene_build_static(&s, m.tris, m.count);
    nav_build(&s.nav, &s.static_bvh);
}

static void free_world(void) {
    mesh_free(&m);
    scene_free(&s);
}

/* Teleport from waypoint to waypoint; true if `to` is reached. Fails if
 * a leg crosses the wall's line within its span. */
static bool walk_path(Vec3 from, Vec3 to, Vec3 *end) {
    Vec3 at = from;
    for (int i = 0; i < MAX_LEGS; i++) {
        Vec3 next;
        if (!nav_next_waypoint(&s.nav, at, to, &next)) {
            return false;
        }
        bool crosses = (at.z - 4) * (next.z - 4) < 0;
        float t = crosses ? (4 - at.z) / (next.z - at.z) : 0;
        if (crosses && fabsf(at.x + (next.x - at.x) * t) < 6.2f) {
            return false;
        }
        at = next;
        if (at.x == to.x && at.y == to.y && at.z == to.z) {
            *end = at;
            return true;
        }
    }
    return false;
}

TEST(test_nav_floor_nodes) {
    build_world();
    int n = nav_node_at(&s.nav, vec3(0.1f, 0, 0.1f));
    ASSERT(n != NAV_NONE);
    Vec3 p = nav_node_pos(&s.nav, n);
    ASSERT_NEAR(p.y, 0.0f, 1e-3f);
    ASSERT(fabsf(p.x - 0.1f) <= NAV_CELL && fabsf(p.z - 0.1f) <= NAV_CELL);

    /* Under the low slab: no headroom, and its top is out of step reach */
    ASSERT(nav_node_at(&s.nav, vec3(-13.6f, 0, -13.6f)) == NAV_NONE);
    free_world();
}

TEST(test_nav_path_around_wall) {
    build_world();
    Vec3 end;
    ASSERT(walk_path(vec3(0, 0, 8), vec3(0, 0, 0), &end));
    free_world();
}

TEST(test_nav_path_up_stairs) {
    build_world();
    Vec3 end;
    ASSERT(walk_path(vec3(0, 0, 0), vec3(0, 1.5f, -9.5f), &end));
    ASSERT_NEAR(nav_node_pos(&s.nav, nav_node_at(&s.nav, end)).y, 1.5f, 1e-3f);
    free_world();
}

TEST(test_nav_crate_unreachable) {
    build_world();
    Vec3 wp;
    ASSERT(nav_node_at(&s.nav, vec3(8.5f, 1, -1.5f)) != NAV_NONE);
    ASSERT(!nav_next_waypoint(&s.nav, vec3(0, 0, 0), vec3(8.5f, 1, -1.5f), &wp));
    free_world();
}

TEST(test_nav_flood_walls_and_doors) {
    build_world();
    nav_flood(&s.nav, vec3(0, 0, 0), 8);
    ASSERT(nav_heard(&s.nav, vec3(-5, 0, -3)));
    ASSERT(!nav_heard(&s.nav, vec3(0, 0, 6)));  /* 6 m away, 14 m around the wall */
    ASSERT(!nav_heard(&s.nav, vec3(-12, 0, 0)));

    /* A closed door band across the hall stops it */
    nav_set_door(&s.nav, vec3(-20, 0, 2.0f), vec3(20, 2, 2.1f), NAV_DOOR_OPENABLE, 1);
    nav_flood(&s.nav, vec3(0, 0, 0), 30);
    ASSERT(!nav_heard(&s.nav, vec3(0, 0, 3)));
    nav_set_door(&s.nav, vec3(-20, 0, 2.0f), vec3(20, 2, 2.1f), NAV_DOOR_OPENABLE, -1);
    nav_flood(&s.nav, vec3(0, 0, 0), 30);
    ASSERT(nav_heard(&s.nav, vec3(0, 0, 3)));
    free_world();
}

/* Openable doors let paths through, locked ones don't */
TEST(test_nav_locked_door_blocks_path) {
    build_world();
    Vec3 lo = vec3(-20, 0, 2.0f), hi = vec3(20, 2, 2.1f);
    Vec3 wp;

    nav_set_door(&s.nav, lo, hi, NAV_DOOR_OPENABLE, 1);
    ASSERT(nav_next_waypoint(&s.nav, vec3(0, 0, 0), vec3(-10, 0, 6), &wp));
    nav_set_door(&s.nav, lo, hi, NAV_DOOR_OPENABLE, -1);

    nav_set_door(&s.nav, lo, hi, NAV_DOOR_LOCKED, 1);
    ASSERT(!nav_next_waypoint(&s.nav, vec3(0, 0, 0), vec3(-10, 0, 6), &wp));
    free_world();
}

TEST(test_nav_empty_world) {
    scene_init(&s);
    nav_build(&s.nav, &s.static_bvh);
    Vec3 wp;
    ASSERT(nav_node_at(&s.nav, vec3(0, 0, 0)) == NAV_NONE);
    ASSERT(!nav_next_waypoint(&s.nav, vec3(0, 0, 0), vec3(1, 0, 1), &wp));
    scene_free(&s);
}

int main(void) {
    TEST_SUITE("nav");
    RUN_TEST(test_nav_floor_nodes);
    RUN_TEST(test_nav_path_around_wall);
    RUN_TEST(test_nav_path_up_stairs);
    RUN_TEST(test_nav_crate_unreachable);
    RUN_TEST(test_nav_flood_walls_and_doors);
    RUN_TEST(test_nav_locked_door_blocks_path);
    RUN_TEST(test_nav_empty_world);
    TEST_REPORT();
}
