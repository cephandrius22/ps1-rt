/*
 * screenfx.h — Screen shake and full-screen color flashes.
 *
 * Presentation only: shake jolts the render camera, never the aim pose, so
 * the simulation stays deterministic. main.c turns game events into
 * requests each frame; effects decay with frame time:
 *
 *   EV_PLAYER_HURT ──> screenfx_flash(red) + small shake
 *   EV_WEAPON_FIRED ─> screenfx_shake(per-weapon amount)
 *
 *   screenfx_update(dt) ──> camera yaw/pitch offset, overlay tint strength
 */
#ifndef SCREENFX_H
#define SCREENFX_H

#include "math3d.h"

typedef struct {
    float shake;        /* current strength, radians of camera jolt */
    float flash;        /* 0..1 tint strength */
    Vec3 flash_color;
} ScreenFx;

/* Requests take the max with what's already running, so a burst of
 * small hits doesn't stack into a huge jolt */
void screenfx_shake(ScreenFx *fx, float amount);
void screenfx_flash(ScreenFx *fx, Vec3 color, float amount);

/* Exponential decay toward zero */
void screenfx_update(ScreenFx *fx, float dt);

/* Camera offsets for this frame: smooth noise from time (sum of sines,
 * no rand()) scaled by the shake strength; 0 when not shaking */
void screenfx_camera_offset(const ScreenFx *fx, float time, float *yaw, float *pitch);

#endif
