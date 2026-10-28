#include "test_framework.h"
#include "camera.h"

TEST(test_camera_create) {
    Camera cam = camera_create(vec3(1, 2, 3), 0.5f, -0.2f, 1.2f);
    ASSERT_NEAR(cam.position.x, 1.0f, 1e-6f);
    ASSERT_NEAR(cam.position.y, 2.0f, 1e-6f);
    ASSERT_NEAR(cam.position.z, 3.0f, 1e-6f);
    ASSERT_NEAR(cam.yaw, 0.5f, 1e-6f);
    ASSERT_NEAR(cam.pitch, -0.2f, 1e-6f);
    ASSERT_NEAR(cam.fov, 1.2f, 1e-6f);
}

TEST(test_camera_ray_center) {
    // Camera at origin looking along -Z
    Camera cam = camera_create(vec3(0, 0, 0), 0.0f, 0.0f, 1.2f);
    Ray ray = camera_ray(&cam, SCREEN_W / 2, SCREEN_H / 2);

    // Center ray should point roughly along -Z
    ASSERT_NEAR(ray.origin.x, 0.0f, 1e-6f);
    ASSERT_NEAR(ray.origin.y, 0.0f, 1e-6f);
    ASSERT_NEAR(ray.origin.z, 0.0f, 1e-6f);
    ASSERT(ray.dir.z < -0.5f);
    ASSERT_NEAR(ray.dir.x, 0.0f, 0.05f);
    ASSERT_NEAR(ray.dir.y, 0.0f, 0.05f);
}

TEST(test_camera_ray_is_normalized) {
    Camera cam = camera_create(vec3(0, 0, 0), 0.3f, 0.1f, 1.2f);

    // Test several pixel positions
    int positions[][2] = {{0, 0}, {SCREEN_W-1, 0}, {0, SCREEN_H-1},
                          {SCREEN_W-1, SCREEN_H-1}, {SCREEN_W/2, SCREEN_H/2}};
    for (int i = 0; i < 5; i++) {
        Ray ray = camera_ray(&cam, positions[i][0], positions[i][1]);
        float len = vec3_length(ray.dir);
        ASSERT_NEAR(len, 1.0f, 1e-5f);
    }
}

TEST(test_camera_ray_corners_diverge) {
    Camera cam = camera_create(vec3(0, 0, 0), 0.0f, 0.0f, 1.2f);
    Ray tl = camera_ray(&cam, 0, 0);
    Ray br = camera_ray(&cam, SCREEN_W - 1, SCREEN_H - 1);

    // Top-left should point up-left, bottom-right down-right
    ASSERT(tl.dir.x < br.dir.x);
    ASSERT(tl.dir.y > br.dir.y);
}

TEST(test_camera_ray_yaw_rotates) {
    Camera cam0 = camera_create(vec3(0, 0, 0), 0.0f, 0.0f, 1.2f);
    Camera cam90 = camera_create(vec3(0, 0, 0), MATH_PI / 2.0f, 0.0f, 1.2f);

    Ray r0 = camera_ray(&cam0, SCREEN_W / 2, SCREEN_H / 2);
    Ray r90 = camera_ray(&cam90, SCREEN_W / 2, SCREEN_H / 2);

    // Default looks along -Z, 90 degree yaw should look along -X
    ASSERT(r0.dir.z < -0.5f);
    ASSERT(r90.dir.x < -0.5f);
}

/* camera_project() inverts camera_ray(): a point along pixel (x, y)'s
 * ray lands back on (x, y) at that distance, for several poses */
TEST(test_camera_project_roundtrip) {
    float yaws[] = {0.0f, 0.7f, -2.0f};
    float pitches[] = {0.0f, 0.4f, -0.3f};
    int pixels[][2] = {{160, 120}, {10, 15}, {300, 220}, {57, 190}};

    for (int c = 0; c < 3; c++) {
        Camera cam = camera_create(vec3(1, 2, 3), yaws[c], pitches[c], 1.2f);
        for (int k = 0; k < 4; k++) {
            Ray r = camera_ray(&cam, pixels[k][0], pixels[k][1]);
            Vec3 p = vec3_add(r.origin, vec3_mul(r.dir, 7.5f));
            int x, y;
            float dist;
            ASSERT(camera_project(&cam, p, &x, &y, &dist));
            ASSERT_EQ_INT(x, pixels[k][0]);
            ASSERT_EQ_INT(y, pixels[k][1]);
            ASSERT_NEAR(dist, 7.5f, 1e-3f);
        }
    }
}

TEST(test_camera_project_rejects_behind) {
    Camera cam = camera_create(vec3(0, 0, 0), 0, 0, 1.2f);
    int x, y;
    float dist;
    ASSERT(!camera_project(&cam, vec3(0, 0, 5), &x, &y, &dist));   /* behind */
    ASSERT(!camera_project(&cam, vec3(100, 0, -1), &x, &y, &dist)); /* off to the side */
}

int main(void) {
    TEST_SUITE("camera");
    RUN_TEST(test_camera_create);
    RUN_TEST(test_camera_ray_center);
    RUN_TEST(test_camera_ray_is_normalized);
    RUN_TEST(test_camera_ray_corners_diverge);
    RUN_TEST(test_camera_ray_yaw_rotates);
    RUN_TEST(test_camera_project_roundtrip);
    RUN_TEST(test_camera_project_rejects_behind);
    TEST_REPORT();
}
