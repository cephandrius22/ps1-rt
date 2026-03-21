#include "test_framework.h"
#include "../src/camera.h"

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
    Camera cam90 = camera_create(vec3(0, 0, 0), M_PI / 2.0f, 0.0f, 1.2f);

    Ray r0 = camera_ray(&cam0, SCREEN_W / 2, SCREEN_H / 2);
    Ray r90 = camera_ray(&cam90, SCREEN_W / 2, SCREEN_H / 2);

    // Default looks along -Z, 90 degree yaw should look along -X
    ASSERT(r0.dir.z < -0.5f);
    ASSERT(r90.dir.x < -0.5f);
}

int main(void) {
    TEST_SUITE("camera");
    RUN_TEST(test_camera_create);
    RUN_TEST(test_camera_ray_center);
    RUN_TEST(test_camera_ray_is_normalized);
    RUN_TEST(test_camera_ray_corners_diverge);
    RUN_TEST(test_camera_ray_yaw_rotates);
    TEST_REPORT();
}
