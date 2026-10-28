/*
 * camera.h — Camera state and ray generation.
 *
 * Generates world-space rays for each pixel based on camera position,
 * orientation (yaw/pitch), and field of view. Rays are cast through a
 * virtual screen at the SCREEN_W x SCREEN_H internal resolution.
 */
#ifndef CAMERA_H
#define CAMERA_H

#include "math3d.h"
#include <stdbool.h>

#define SCREEN_W 320
#define SCREEN_H 240

typedef struct {
    Vec3 position;
    float yaw;
    float pitch;
    float fov;
} Camera;

/* fov is the vertical field of view in radians */
Camera camera_create(Vec3 pos, float yaw, float pitch, float fov);
Camera camera_from_pose(Pose pose, float fov);

/* World ray through the center of pixel (px, py); (0, 0) is top-left */
Ray camera_ray(const Camera *cam, int px, int py);

/* Inverse of camera_ray(): the pixel a world point lands on, and its
 * distance from the eye (comparable with a ray's hit t). False if the
 * point is behind the camera, too close, or off-screen. */
bool camera_project(const Camera *cam, Vec3 p, int *px, int *py, float *dist);

/* Pixels per meter at distance 1 (for sizing things drawn by projection) */
float camera_focal_px(const Camera *cam);

#endif
