/*
 * player.h — First-person player controller with physics and collision.
 *
 * Translates input state into camera movement (yaw-relative WASD) and
 * mouse look (yaw/pitch with clamping). Includes gravity, ground snapping,
 * wall collision with sliding, and step climbing.
 */
#ifndef PLAYER_H
#define PLAYER_H

#include "camera.h"
#include "input.h"
#include "bvh.h"

struct Scene;

#define PLAYER_HEIGHT     1.6f
#define PLAYER_EYE_OFFSET 0.1f
#define PLAYER_RADIUS     0.3f
#define PLAYER_SPEED      5.0f
#define PLAYER_GRAVITY    18.0f
#define PLAYER_JUMP_VEL   7.0f
#define STEP_HEIGHT       0.6f
#define MOUSE_SENSITIVITY 0.003f

typedef struct {
    Camera cam;
    Vec3 velocity;
    float foot_y;
    bool on_ground;
    bool flashlight_on;
    float walk_distance;  /* accumulates distance walked for footstep sounds */
    float collected;      /* amount collected this frame (0 if none) */
    int door_action;      /* 1=opened, -1=closed, 0=none this frame */
} Player;

Player player_create(Vec3 pos);
void player_update(Player *p, const InputState *input, float dt, const BVH *world);
void player_update_scene(Player *p, const InputState *input, float dt, struct Scene *scene);

#endif
