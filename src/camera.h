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

#define SCREEN_W 320
#define SCREEN_H 240

typedef struct {
    Vec3 position;
    float yaw;
    float pitch;
    float fov;
} Camera;

Camera camera_create(Vec3 pos, float yaw, float pitch, float fov);
Ray camera_ray(const Camera *cam, int px, int py);

#endif
