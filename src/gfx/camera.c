#include "camera.h"

Camera camera_create(Vec3 pos, float yaw, float pitch, float fov) {
    return (Camera){pos, yaw, pitch, fov};
}

Camera camera_from_pose(Pose pose, float fov) {
    return camera_create(pose.pos, pose.yaw, pose.pitch, fov);
}

Ray camera_ray(const Camera *cam, int px, int py) {
    float aspect = (float)SCREEN_W / (float)SCREEN_H;
    float half_fov = tanf(cam->fov * 0.5f);

    float ndc_x = (2.0f * ((float)px + 0.5f) / (float)SCREEN_W - 1.0f) * aspect * half_fov;
    float ndc_y = (1.0f - 2.0f * ((float)py + 0.5f) / (float)SCREEN_H) * half_fov;

    Pose pose = {cam->position, cam->yaw, cam->pitch};
    return (Ray){cam->position, pose_to_world_dir(pose, vec3(ndc_x, ndc_y, -1.0f))};
}

#define CAMERA_NEAR 0.05f /* closer than this can't be projected sensibly */

float camera_focal_px(const Camera *cam) {
    return (SCREEN_H * 0.5f) / tanf(cam->fov * 0.5f);
}

bool camera_to_screen(const Camera *cam, Vec3 p, float *screen_x, float *screen_y) {
    /* Into view space: undo yaw (about Y), then pitch (about X). This
     * inverts pose_to_world_dir(), which applies pitch then yaw. */
    Vec3 d = vec3_sub(p, cam->position);
    float cy = cosf(-cam->yaw), sy = sinf(-cam->yaw);
    Vec3 a = vec3(d.x * cy + d.z * sy, d.y, -d.x * sy + d.z * cy);
    float cp = cosf(-cam->pitch), sp = sinf(-cam->pitch);
    Vec3 v = vec3(a.x, a.y * cp - a.z * sp, a.y * sp + a.z * cp);
    if (v.z > -CAMERA_NEAR) {
        return false;
    }

    /* Same mapping as camera_ray(), solved for the pixel */
    float aspect = (float)SCREEN_W / (float)SCREEN_H;
    float half_fov = tanf(cam->fov * 0.5f);
    float ndc_x = (v.x / -v.z) / (aspect * half_fov);
    float ndc_y = (v.y / -v.z) / half_fov;
    *screen_x = (ndc_x + 1.0f) * 0.5f * SCREEN_W;
    *screen_y = (1.0f - ndc_y) * 0.5f * SCREEN_H;
    return true;
}

bool camera_project(const Camera *cam, Vec3 p, int *px, int *py, float *dist) {
    float fx, fy;
    if (!camera_to_screen(cam, p, &fx, &fy)) {
        return false;
    }
    if (fx < 0 || fx >= SCREEN_W || fy < 0 || fy >= SCREEN_H) {
        return false;
    }

    *px = (int)fx;
    *py = (int)fy;
    *dist = vec3_length(vec3_sub(p, cam->position));
    return true;
}
