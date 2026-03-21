#include "test_framework.h"
#include "../src/bvh.h"

static Triangle make_tri_at(float x, float y, float z) {
    return (Triangle){
        .v0 = vec3(x - 0.5f, y, z - 0.5f),
        .v1 = vec3(x + 0.5f, y, z - 0.5f),
        .v2 = vec3(x, y + 1.0f, z),
        .uv0 = {0, 0}, .uv1 = {1, 0}, .uv2 = {0.5f, 1},
        .normal = vec3(0, 0, 1), .material_id = 0
    };
}

TEST(test_aabb_hit_basic) {
    AABB box = {vec3(-1, -1, -1), vec3(1, 1, 1)};
    Ray ray = {vec3(0, 0, 5), vec3(0, 0, -1)};
    ASSERT(aabb_hit(box, ray, 0.001f, 1000.0f));
}

TEST(test_aabb_miss) {
    AABB box = {vec3(-1, -1, -1), vec3(1, 1, 1)};
    Ray ray = {vec3(5, 5, 5), vec3(1, 0, 0)};
    ASSERT(!aabb_hit(box, ray, 0.001f, 1000.0f));
}

TEST(test_aabb_hit_from_inside) {
    AABB box = {vec3(-1, -1, -1), vec3(1, 1, 1)};
    Ray ray = {vec3(0, 0, 0), vec3(1, 0, 0)};
    ASSERT(aabb_hit(box, ray, 0.001f, 1000.0f));
}

TEST(test_aabb_hit_axis_aligned) {
    AABB box = {vec3(2, 2, 2), vec3(4, 4, 4)};
    // Ray along X axis at y=3, z=3
    Ray ray = {vec3(0, 3, 3), vec3(1, 0, 0)};
    ASSERT(aabb_hit(box, ray, 0.001f, 1000.0f));
}

TEST(test_bvh_build_empty) {
    BVH bvh;
    bvh_build(&bvh, NULL, 0);
    ASSERT_EQ_INT(bvh.node_count, 0);

    Ray ray = {vec3(0, 0, 0), vec3(0, 0, -1)};
    HitRecord rec = bvh_intersect(&bvh, ray, 0.001f, 1000.0f);
    ASSERT(!rec.hit);

    bvh_free(&bvh);
}

TEST(test_bvh_build_single) {
    Triangle tri = make_tri_at(0, 0, 0);
    BVH bvh;
    bvh_build(&bvh, &tri, 1);
    ASSERT(bvh.node_count > 0);
    ASSERT_EQ_INT(bvh.tri_count, 1);
    bvh_free(&bvh);
}

TEST(test_bvh_build_many) {
    Triangle tris[20];
    for (int i = 0; i < 20; i++)
        tris[i] = make_tri_at((float)(i % 5) * 3.0f, 0, (float)(i / 5) * 3.0f);

    BVH bvh;
    bvh_build(&bvh, tris, 20);
    ASSERT(bvh.node_count > 1);  // Should have internal nodes
    ASSERT_EQ_INT(bvh.tri_count, 20);
    bvh_free(&bvh);
}

TEST(test_bvh_intersect_hit) {
    // Floor triangle
    Triangle tri = {
        .v0 = vec3(-10, 0, -10), .v1 = vec3(10, 0, -10), .v2 = vec3(10, 0, 10),
        .normal = vec3(0, 1, 0), .material_id = 0
    };
    BVH bvh;
    bvh_build(&bvh, &tri, 1);

    Ray ray = {vec3(0, 5, 0), vec3(0, -1, 0)};
    HitRecord rec = bvh_intersect(&bvh, ray, 0.001f, 1000.0f);
    ASSERT(rec.hit);
    ASSERT_NEAR(rec.t, 5.0f, 1e-4f);

    bvh_free(&bvh);
}

TEST(test_bvh_intersect_miss) {
    Triangle tri = make_tri_at(0, 0, -5);
    BVH bvh;
    bvh_build(&bvh, &tri, 1);

    // Ray that misses entirely
    Ray ray = {vec3(100, 100, 100), vec3(0, 0, 1)};
    HitRecord rec = bvh_intersect(&bvh, ray, 0.001f, 1000.0f);
    ASSERT(!rec.hit);

    bvh_free(&bvh);
}

TEST(test_bvh_matches_brute_force) {
    // Build a scene with multiple triangles, verify BVH gives same result as brute force
    Triangle tris[8];
    for (int i = 0; i < 8; i++) {
        float x = (float)(i % 4) * 2.0f - 3.0f;
        float z = (float)(i / 4) * 2.0f - 5.0f;
        tris[i] = (Triangle){
            .v0 = vec3(x - 0.5f, 0, z - 0.5f),
            .v1 = vec3(x + 0.5f, 0, z - 0.5f),
            .v2 = vec3(x, 0, z + 0.5f),
            .normal = vec3(0, 1, 0), .material_id = i
        };
    }

    BVH bvh;
    bvh_build(&bvh, tris, 8);

    Mesh m;
    mesh_init(&m);
    for (int i = 0; i < 8; i++)
        mesh_add_tri(&m, tris[i]);

    // Test multiple rays
    float test_x[] = {-3, -1, 1, 3, 0, -2, 2};
    for (int i = 0; i < 7; i++) {
        Ray ray = {vec3(test_x[i], 3, -4), vec3(0, -1, 0)};

        HitRecord bvh_rec = bvh_intersect(&bvh, ray, 0.001f, 1000.0f);
        HitRecord mesh_rec = mesh_intersect(&m, ray, 0.001f, 1000.0f);

        ASSERT(bvh_rec.hit == mesh_rec.hit);
        if (bvh_rec.hit) {
            ASSERT_NEAR(bvh_rec.t, mesh_rec.t, 1e-4f);
        }
    }

    mesh_free(&m);
    bvh_free(&bvh);
}

TEST(test_bvh_closest_hit) {
    // Two triangles at different distances
    Triangle tris[2] = {
        {.v0 = vec3(-5, 2, -5), .v1 = vec3(5, 2, -5), .v2 = vec3(5, 2, 5),
         .normal = vec3(0, 1, 0), .material_id = 0},
        {.v0 = vec3(-5, 0, -5), .v1 = vec3(5, 0, -5), .v2 = vec3(5, 0, 5),
         .normal = vec3(0, 1, 0), .material_id = 1}
    };

    BVH bvh;
    bvh_build(&bvh, tris, 2);

    Ray ray = {vec3(0, 5, 0), vec3(0, -1, 0)};
    HitRecord rec = bvh_intersect(&bvh, ray, 0.001f, 1000.0f);
    ASSERT(rec.hit);
    ASSERT_NEAR(rec.t, 3.0f, 1e-4f);
    ASSERT_EQ_INT(rec.material_id, 0);

    bvh_free(&bvh);
}

int main(void) {
    TEST_SUITE("AABB intersection");
    RUN_TEST(test_aabb_hit_basic);
    RUN_TEST(test_aabb_miss);
    RUN_TEST(test_aabb_hit_from_inside);
    RUN_TEST(test_aabb_hit_axis_aligned);

    TEST_SUITE("BVH construction");
    RUN_TEST(test_bvh_build_empty);
    RUN_TEST(test_bvh_build_single);
    RUN_TEST(test_bvh_build_many);

    TEST_SUITE("BVH intersection");
    RUN_TEST(test_bvh_intersect_hit);
    RUN_TEST(test_bvh_intersect_miss);
    RUN_TEST(test_bvh_matches_brute_force);
    RUN_TEST(test_bvh_closest_hit);
    TEST_REPORT();
}
