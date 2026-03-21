#include "player.h"
#include <math.h>

Player player_create(Vec3 pos) {
    Player p;
    p.cam = camera_create(pos, 0.0f, 0.0f, 1.2f);
    p.velocity = vec3(0, 0, 0);
    return p;
}

void player_update(Player *p, const InputState *input, float dt) {
    // Mouse look
    p->cam.yaw -= input->mouse_dx * MOUSE_SENSITIVITY;
    p->cam.pitch -= input->mouse_dy * MOUSE_SENSITIVITY;

    // Clamp pitch
    if (p->cam.pitch > 1.4f) p->cam.pitch = 1.4f;
    if (p->cam.pitch < -1.4f) p->cam.pitch = -1.4f;

    // Movement relative to camera yaw
    Vec3 forward = vec3(-sinf(p->cam.yaw), 0, -cosf(p->cam.yaw));
    Vec3 right = vec3(cosf(p->cam.yaw), 0, -sinf(p->cam.yaw));

    Vec3 move = vec3(0, 0, 0);
    if (input->forward) move = vec3_add(move, forward);
    if (input->back)    move = vec3_sub(move, forward);
    if (input->right)   move = vec3_add(move, right);
    if (input->left)    move = vec3_sub(move, right);

    if (vec3_dot(move, move) > 0.001f)
        move = vec3_normalize(move);

    p->cam.position = vec3_add(p->cam.position, vec3_mul(move, PLAYER_SPEED * dt));

    // Keep player at fixed height for now
    p->cam.position.y = PLAYER_HEIGHT;
}
