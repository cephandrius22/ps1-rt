#include "test_framework.h"
#include "../src/mesh.h"

static Triangle make_floor_tri(void) {
    Triangle tri = {
        .v0 = vec3(-5, 0, -5), .v1 = vec3(5, 0, -5), .v2 = vec3(5, 0, 5),
        .uv0 = {0, 0}, .uv1 = {1, 0}, .uv2 = {1, 1},
        .normal = vec3(0, 1, 0), .material_id = 0
    };
    return tri;
}

TEST(test_ray_tri_hit) {
    Triangle tri = make_floor_tri();
    Ray ray = {vec3(0, 5, 0), vec3_normalize(vec3(0, -1, 0))};
    HitRecord rec;
    bool hit = ray_triangle_intersect(ray, &tri, 0.001f, 1000.0f, &rec);
    ASSERT(hit);
    ASSERT_NEAR(rec.t, 5.0f, 1e-4f);
    ASSERT_NEAR(rec.point.y, 0.0f, 1e-4f);
    ASSERT_NEAR(rec.normal.y, 1.0f, 1e-6f);
}

TEST(test_ray_tri_miss) {
    Triangle tri = make_floor_tri();
    // Ray shooting upward from below
    Ray ray = {vec3(0, -1, 0), vec3_normalize(vec3(0, -1, 0))};
    HitRecord rec;
    bool hit = ray_triangle_intersect(ray, &tri, 0.001f, 1000.0f, &rec);
    ASSERT(!hit);
}

TEST(test_ray_tri_miss_outside) {
    Triangle tri = make_floor_tri();
    // Ray far outside triangle bounds
    Ray ray = {vec3(100, 5, 100), vec3_normalize(vec3(0, -1, 0))};
    HitRecord rec;
    bool hit = ray_triangle_intersect(ray, &tri, 0.001f, 1000.0f, &rec);
    ASSERT(!hit);
}

TEST(test_ray_tri_behind) {
    Triangle tri = make_floor_tri();
    // Ray origin below floor, pointing down (tri is behind ray)
    Ray ray = {vec3(0, -1, 0), vec3(0, -1, 0)};
    HitRecord rec;
    bool hit = ray_triangle_intersect(ray, &tri, 0.001f, 1000.0f, &rec);
    ASSERT(!hit);
}

TEST(test_ray_tri_t_range) {
    Triangle tri = make_floor_tri();
    Ray ray = {vec3(0, 5, 0), vec3_normalize(vec3(0, -1, 0))};
    HitRecord rec;

    // Hit is at t=5, but t_max is 3
    bool hit = ray_triangle_intersect(ray, &tri, 0.001f, 3.0f, &rec);
    ASSERT(!hit);

    // Hit is at t=5, but t_min is 6
    hit = ray_triangle_intersect(ray, &tri, 6.0f, 1000.0f, &rec);
    ASSERT(!hit);
}

TEST(test_ray_tri_uv_interpolation) {
    Triangle tri = {
        .v0 = vec3(0, 0, -5), .v1 = vec3(2, 0, -5), .v2 = vec3(0, 2, -5),
        .uv0 = {0, 0}, .uv1 = {1, 0}, .uv2 = {0, 1},
        .normal = vec3(0, 0, 1), .material_id = 0
    };
    // Ray hitting center of triangle
    Vec3 center = vec3_mul(vec3_add(vec3_add(tri.v0, tri.v1), tri.v2), 1.0f / 3.0f);
    Ray ray = {vec3_add(center, vec3(0, 0, 5)), vec3(0, 0, -1)};
    HitRecord rec;
    bool hit = ray_triangle_intersect(ray, &tri, 0.001f, 1000.0f, &rec);
    ASSERT(hit);
    // At centroid, barycentric coords are (1/3, 1/3, 1/3)
    ASSERT_NEAR(rec.uv.u, 1.0f / 3.0f, 0.05f);
    ASSERT_NEAR(rec.uv.v, 1.0f / 3.0f, 0.05f);
}

TEST(test_mesh_init_and_add) {
    Mesh m;
    mesh_init(&m);
    ASSERT_EQ_INT(m.count, 0);
    ASSERT(m.capacity > 0);

    Triangle tri = make_floor_tri();
    mesh_add_tri(&m, tri);
    ASSERT_EQ_INT(m.count, 1);

    mesh_free(&m);
    ASSERT_EQ_INT(m.count, 0);
    ASSERT_EQ_INT(m.capacity, 0);
    ASSERT(m.tris == NULL);
}

TEST(test_mesh_grow) {
    Mesh m;
    mesh_init(&m);
    Triangle tri = make_floor_tri();

    for (int i = 0; i < 200; i++)
        mesh_add_tri(&m, tri);

    ASSERT_EQ_INT(m.count, 200);
    ASSERT(m.capacity >= 200);
    mesh_free(&m);
}

TEST(test_mesh_intersect) {
    Mesh m;
    mesh_init(&m);

    // Two triangles forming a floor
    mesh_add_tri(&m, (Triangle){
        .v0 = vec3(-5, 0, -5), .v1 = vec3(5, 0, -5), .v2 = vec3(5, 0, 5),
        .normal = vec3(0, 1, 0), .material_id = 0
    });
    mesh_add_tri(&m, (Triangle){
        .v0 = vec3(-5, 0, -5), .v1 = vec3(5, 0, 5), .v2 = vec3(-5, 0, 5),
        .normal = vec3(0, 1, 0), .material_id = 0
    });

    Ray ray = {vec3(0, 3, 0), vec3(0, -1, 0)};
    HitRecord rec = mesh_intersect(&m, ray, 0.001f, 1000.0f);
    ASSERT(rec.hit);
    ASSERT_NEAR(rec.t, 3.0f, 1e-4f);

    // Miss
    Ray miss_ray = {vec3(100, 3, 100), vec3(0, -1, 0)};
    rec = mesh_intersect(&m, miss_ray, 0.001f, 1000.0f);
    ASSERT(!rec.hit);

    mesh_free(&m);
}

TEST(test_mesh_intersect_closest) {
    Mesh m;
    mesh_init(&m);

    // Two horizontal planes at different heights
    mesh_add_tri(&m, (Triangle){
        .v0 = vec3(-5, 2, -5), .v1 = vec3(5, 2, -5), .v2 = vec3(5, 2, 5),
        .normal = vec3(0, 1, 0), .material_id = 0
    });
    mesh_add_tri(&m, (Triangle){
        .v0 = vec3(-5, 0, -5), .v1 = vec3(5, 0, -5), .v2 = vec3(5, 0, 5),
        .normal = vec3(0, 1, 0), .material_id = 1
    });

    Ray ray = {vec3(0, 5, 0), vec3(0, -1, 0)};
    HitRecord rec = mesh_intersect(&m, ray, 0.001f, 1000.0f);
    ASSERT(rec.hit);
    ASSERT_NEAR(rec.t, 3.0f, 1e-4f);  // Should hit y=2 plane first
    ASSERT_EQ_INT(rec.material_id, 0);

    mesh_free(&m);
}

int main(void) {
    TEST_SUITE("ray-triangle intersection");
    RUN_TEST(test_ray_tri_hit);
    RUN_TEST(test_ray_tri_miss);
    RUN_TEST(test_ray_tri_miss_outside);
    RUN_TEST(test_ray_tri_behind);
    RUN_TEST(test_ray_tri_t_range);
    RUN_TEST(test_ray_tri_uv_interpolation);

    TEST_SUITE("mesh");
    RUN_TEST(test_mesh_init_and_add);
    RUN_TEST(test_mesh_grow);
    RUN_TEST(test_mesh_intersect);
    RUN_TEST(test_mesh_intersect_closest);
    TEST_REPORT();
}
