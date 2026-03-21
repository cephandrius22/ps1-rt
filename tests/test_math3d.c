#include "test_framework.h"
#include "../src/math3d.h"

TEST(test_vec3_add) {
    Vec3 a = vec3(1, 2, 3);
    Vec3 b = vec3(4, 5, 6);
    Vec3 r = vec3_add(a, b);
    ASSERT_NEAR(r.x, 5.0f, 1e-6f);
    ASSERT_NEAR(r.y, 7.0f, 1e-6f);
    ASSERT_NEAR(r.z, 9.0f, 1e-6f);
}

TEST(test_vec3_sub) {
    Vec3 r = vec3_sub(vec3(5, 3, 1), vec3(1, 2, 3));
    ASSERT_NEAR(r.x, 4.0f, 1e-6f);
    ASSERT_NEAR(r.y, 1.0f, 1e-6f);
    ASSERT_NEAR(r.z, -2.0f, 1e-6f);
}

TEST(test_vec3_mul) {
    Vec3 r = vec3_mul(vec3(2, 3, 4), 2.0f);
    ASSERT_NEAR(r.x, 4.0f, 1e-6f);
    ASSERT_NEAR(r.y, 6.0f, 1e-6f);
    ASSERT_NEAR(r.z, 8.0f, 1e-6f);
}

TEST(test_vec3_neg) {
    Vec3 r = vec3_neg(vec3(1, -2, 3));
    ASSERT_NEAR(r.x, -1.0f, 1e-6f);
    ASSERT_NEAR(r.y, 2.0f, 1e-6f);
    ASSERT_NEAR(r.z, -3.0f, 1e-6f);
}

TEST(test_vec3_dot) {
    float d = vec3_dot(vec3(1, 0, 0), vec3(0, 1, 0));
    ASSERT_NEAR(d, 0.0f, 1e-6f);

    d = vec3_dot(vec3(1, 0, 0), vec3(1, 0, 0));
    ASSERT_NEAR(d, 1.0f, 1e-6f);

    d = vec3_dot(vec3(1, 2, 3), vec3(4, 5, 6));
    ASSERT_NEAR(d, 32.0f, 1e-6f);
}

TEST(test_vec3_cross) {
    Vec3 r = vec3_cross(vec3(1, 0, 0), vec3(0, 1, 0));
    ASSERT_NEAR(r.x, 0.0f, 1e-6f);
    ASSERT_NEAR(r.y, 0.0f, 1e-6f);
    ASSERT_NEAR(r.z, 1.0f, 1e-6f);

    r = vec3_cross(vec3(0, 1, 0), vec3(1, 0, 0));
    ASSERT_NEAR(r.z, -1.0f, 1e-6f);
}

TEST(test_vec3_length) {
    ASSERT_NEAR(vec3_length(vec3(3, 4, 0)), 5.0f, 1e-6f);
    ASSERT_NEAR(vec3_length(vec3(1, 0, 0)), 1.0f, 1e-6f);
    ASSERT_NEAR(vec3_length(vec3(0, 0, 0)), 0.0f, 1e-6f);
}

TEST(test_vec3_normalize) {
    Vec3 r = vec3_normalize(vec3(3, 0, 0));
    ASSERT_NEAR(r.x, 1.0f, 1e-6f);
    ASSERT_NEAR(r.y, 0.0f, 1e-6f);
    ASSERT_NEAR(r.z, 0.0f, 1e-6f);

    r = vec3_normalize(vec3(1, 1, 1));
    float expected = 1.0f / sqrtf(3.0f);
    ASSERT_NEAR(r.x, expected, 1e-6f);
    ASSERT_NEAR(r.y, expected, 1e-6f);
    ASSERT_NEAR(r.z, expected, 1e-6f);

    // Zero vector stays zero
    r = vec3_normalize(vec3(0, 0, 0));
    ASSERT_NEAR(vec3_length(r), 0.0f, 1e-6f);
}

TEST(test_vec3_lerp) {
    Vec3 r = vec3_lerp(vec3(0, 0, 0), vec3(10, 20, 30), 0.5f);
    ASSERT_NEAR(r.x, 5.0f, 1e-6f);
    ASSERT_NEAR(r.y, 10.0f, 1e-6f);
    ASSERT_NEAR(r.z, 15.0f, 1e-6f);

    r = vec3_lerp(vec3(0, 0, 0), vec3(10, 20, 30), 0.0f);
    ASSERT_NEAR(r.x, 0.0f, 1e-6f);

    r = vec3_lerp(vec3(0, 0, 0), vec3(10, 20, 30), 1.0f);
    ASSERT_NEAR(r.x, 10.0f, 1e-6f);
}

TEST(test_clampf) {
    ASSERT_NEAR(clampf(0.5f, 0.0f, 1.0f), 0.5f, 1e-6f);
    ASSERT_NEAR(clampf(-1.0f, 0.0f, 1.0f), 0.0f, 1e-6f);
    ASSERT_NEAR(clampf(2.0f, 0.0f, 1.0f), 1.0f, 1e-6f);
}

TEST(test_mat4_identity) {
    Mat4 m = mat4_identity();
    ASSERT_NEAR(m.m[0][0], 1.0f, 1e-6f);
    ASSERT_NEAR(m.m[1][1], 1.0f, 1e-6f);
    ASSERT_NEAR(m.m[2][2], 1.0f, 1e-6f);
    ASSERT_NEAR(m.m[3][3], 1.0f, 1e-6f);
    ASSERT_NEAR(m.m[0][1], 0.0f, 1e-6f);
    ASSERT_NEAR(m.m[1][0], 0.0f, 1e-6f);
}

TEST(test_mat4_mul_identity) {
    Mat4 a = mat4_identity();
    Mat4 b = mat4_identity();
    Mat4 r = mat4_mul(a, b);
    ASSERT_NEAR(r.m[0][0], 1.0f, 1e-6f);
    ASSERT_NEAR(r.m[1][1], 1.0f, 1e-6f);
    ASSERT_NEAR(r.m[0][1], 0.0f, 1e-6f);
}

TEST(test_mat4_mul_vec3_identity) {
    Mat4 m = mat4_identity();
    Vec3 v = vec3(1, 2, 3);
    Vec3 r = mat4_mul_vec3(m, v, 0.0f);
    ASSERT_NEAR(r.x, 1.0f, 1e-6f);
    ASSERT_NEAR(r.y, 2.0f, 1e-6f);
    ASSERT_NEAR(r.z, 3.0f, 1e-6f);
}

TEST(test_mat4_rotation_y) {
    // 90 degree rotation around Y: (1,0,0) -> (0,0,-1)
    Mat4 m = mat4_rotation_y(M_PI / 2.0f);
    Vec3 r = mat4_mul_vec3(m, vec3(1, 0, 0), 0.0f);
    ASSERT_NEAR(r.x, 0.0f, 1e-5f);
    ASSERT_NEAR(r.y, 0.0f, 1e-5f);
    ASSERT_NEAR(r.z, -1.0f, 1e-5f);
}

TEST(test_mat4_rotation_x) {
    // 90 degree rotation around X: (0,1,0) -> (0,0,1)
    Mat4 m = mat4_rotation_x(M_PI / 2.0f);
    Vec3 r = mat4_mul_vec3(m, vec3(0, 1, 0), 0.0f);
    ASSERT_NEAR(r.x, 0.0f, 1e-5f);
    ASSERT_NEAR(r.y, 0.0f, 1e-5f);
    ASSERT_NEAR(r.z, 1.0f, 1e-5f);
}

int main(void) {
    TEST_SUITE("math3d");
    RUN_TEST(test_vec3_add);
    RUN_TEST(test_vec3_sub);
    RUN_TEST(test_vec3_mul);
    RUN_TEST(test_vec3_neg);
    RUN_TEST(test_vec3_dot);
    RUN_TEST(test_vec3_cross);
    RUN_TEST(test_vec3_length);
    RUN_TEST(test_vec3_normalize);
    RUN_TEST(test_vec3_lerp);
    RUN_TEST(test_clampf);
    RUN_TEST(test_mat4_identity);
    RUN_TEST(test_mat4_mul_identity);
    RUN_TEST(test_mat4_mul_vec3_identity);
    RUN_TEST(test_mat4_rotation_y);
    RUN_TEST(test_mat4_rotation_x);
    TEST_REPORT();
}
