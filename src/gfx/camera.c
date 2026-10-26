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
