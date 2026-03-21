/*
 * player.h — First-person player controller.
 *
 * Translates input state into camera movement (yaw-relative WASD) and
 * mouse look (yaw/pitch with clamping). Movement speed is delta-time
 * scaled and diagonal movement is normalized to prevent speed boost.
 */
#ifndef PLAYER_H
#define PLAYER_H

#include "camera.h"
#include "input.h"

#define PLAYER_HEIGHT 1.6f
#define PLAYER_SPEED 4.0f
#define MOUSE_SENSITIVITY 0.003f

typedef struct {
    Camera cam;
    Vec3 velocity;
} Player;

Player player_create(Vec3 pos);
void player_update(Player *p, const InputState *input, float dt);

#endif
