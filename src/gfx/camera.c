#include "camera.h"

Camera camera_create(Vec3 pos, float yaw, float pitch, float fov) {
    return (Camera){pos, yaw, pitch, fov};
}

/* View-space direction (-Z forward) → world-space ray from the eye */
static Ray camera_view_ray(const Camera *cam, Vec3 view_dir) {
    Vec3 local_dir = vec3_normalize(view_dir);
    Mat4 rot = mat4_mul(mat4_rotation_y(cam->yaw), mat4_rotation_x(cam->pitch));
    Vec3 world_dir = vec3_normalize(mat4_mul_vec3(rot, local_dir, 0.0f));
    return (Ray){cam->position, world_dir};
}

Ray camera_ray(const Camera *cam, int px, int py) {
    float aspect = (float)SCREEN_W / (float)SCREEN_H;
    float half_fov = tanf(cam->fov * 0.5f);

    float ndc_x = (2.0f * ((float)px + 0.5f) / (float)SCREEN_W - 1.0f) * aspect * half_fov;
    float ndc_y = (1.0f - 2.0f * ((float)py + 0.5f) / (float)SCREEN_H) * half_fov;

    return camera_view_ray(cam, vec3(ndc_x, ndc_y, -1.0f));
}

Ray camera_ray_offset(const Camera *cam, float right, float up) {
    return camera_view_ray(cam, vec3(tanf(right), tanf(up), -1.0f));
}
