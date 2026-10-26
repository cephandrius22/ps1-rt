#include "player.h"
#include "scene.h"
#include <math.h>

Player player_create(Vec3 pos) {
    Player p;
    p.foot_y = pos.y - PLAYER_HEIGHT - PLAYER_EYE_OFFSET;
    p.cam = camera_create(pos, 0.0f, 0.0f, 1.2f);
    p.velocity = vec3(0, 0, 0);
    p.on_ground = false;
    p.flashlight_on = false;
    p.walk_distance = 0;
    p.collected = 0;
    p.door_action = 0;
    return p;
}

static float ground_check(Vec3 head_pos, const BVH *world) {
    Ray down = {vec3(head_pos.x, head_pos.y + STEP_HEIGHT + 0.1f, head_pos.z),
                vec3(0, -1, 0)};
    HitRecord rec = bvh_intersect(world, down, 0.0f, PLAYER_HEIGHT + STEP_HEIGHT + 1.0f);
    if (rec.hit && rec.normal.y > 0.5f)
        return rec.point.y;
    return -1000.0f;
}

static bool wall_check(Vec3 from, Vec3 dir, float dist, const BVH *world, HitRecord *out) {
    Ray ray = {from, dir};
    *out = bvh_intersect(world, ray, 0.0f, dist);
    return out->hit;
}

static Vec3 try_move_axis(Vec3 pos, Vec3 move, float foot_y, const BVH *world) {
    float move_len = vec3_length(move);
    if (move_len < 1e-6f) return pos;

    Vec3 move_dir = vec3_mul(move, 1.0f / move_len);
    float check_dist = move_len + PLAYER_RADIUS;

    // Cast rays at knee and chest height
    float check_heights[] = {foot_y + 0.2f, foot_y + PLAYER_HEIGHT * 0.5f, foot_y + PLAYER_HEIGHT - 0.1f};

    for (int i = 0; i < 3; i++) {
        Vec3 ray_origin = vec3(pos.x, check_heights[i], pos.z);
        HitRecord rec;
        if (wall_check(ray_origin, move_dir, check_dist, world, &rec)) {
            float allowed = rec.t - PLAYER_RADIUS;
            if (allowed < 0) allowed = 0;
            if (allowed < move_len)
                move_len = allowed;
        }
    }

    return vec3_add(pos, vec3_mul(move_dir, move_len));
}

/* Apply look deltas, clamping pitch so the view can't flip over */
static void apply_look(Player *p, const PlayerCmd *cmd) {
    p->cam.yaw += cmd->yaw_delta;
    p->cam.pitch += cmd->pitch_delta;
    if (p->cam.pitch > PLAYER_PITCH_MAX) {
        p->cam.pitch = PLAYER_PITCH_MAX;
    }
    if (p->cam.pitch < -PLAYER_PITCH_MAX) {
        p->cam.pitch = -PLAYER_PITCH_MAX;
    }
}

/* Horizontal forward direction for the current yaw (yaw=0 → -Z) */
static Vec3 yaw_forward(const Player *p) {
    return vec3(-sinf(p->cam.yaw), 0, -cosf(p->cam.yaw));
}

/* Yaw-relative wish direction. Length capped at 1 so diagonals aren't
 * faster, while partial (analog) input still moves slower. */
static Vec3 wish_dir(const Player *p, const PlayerCmd *cmd) {
    Vec3 forward = yaw_forward(p);
    Vec3 right = vec3(cosf(p->cam.yaw), 0, -sinf(p->cam.yaw));
    Vec3 dir = vec3_add(vec3_mul(forward, cmd->forward), vec3_mul(right, cmd->strafe));

    if (vec3_dot(dir, dir) > 1.0f) {
        dir = vec3_normalize(dir);
    }
    return dir;
}

void player_update(Player *p, const PlayerCmd *cmd, float dt, const BVH *world) {
    apply_look(p, cmd);
    Vec3 horizontal_move = vec3_mul(wish_dir(p, cmd), PLAYER_SPEED * dt);

    // Jump
    if (cmd_has(cmd, BTN_JUMP) && p->on_ground) {
        p->velocity.y = PLAYER_JUMP_VEL;
        p->on_ground = false;
    }

    // Gravity
    if (!p->on_ground)
        p->velocity.y -= PLAYER_GRAVITY * dt;

    // Apply horizontal movement with wall collision (slide along each axis)
    Vec3 pos = p->cam.position;

    if (world) {
        // Move X axis
        Vec3 move_x = vec3(horizontal_move.x, 0, 0);
        pos = try_move_axis(pos, move_x, p->foot_y, world);

        // Move Z axis
        Vec3 move_z = vec3(0, 0, horizontal_move.z);
        pos = try_move_axis(pos, move_z, p->foot_y, world);
    } else {
        pos = vec3_add(pos, horizontal_move);
    }

    // Apply vertical velocity
    p->foot_y += p->velocity.y * dt;

    // Ground check
    if (world) {
        float ground_y = ground_check(vec3(pos.x, p->foot_y + PLAYER_HEIGHT, pos.z), world);

        if (p->foot_y <= ground_y) {
            p->foot_y = ground_y;
            if (p->velocity.y <= 0) {
                p->velocity.y = 0;
                p->on_ground = true;
            }
        } else if (p->foot_y > ground_y + 0.1f) {
            p->on_ground = false;
        }
    } else {
        // No world: clamp to y=0 floor
        if (p->foot_y < 0) {
            p->foot_y = 0;
            p->velocity.y = 0;
            p->on_ground = true;
        }
    }

    pos.y = p->foot_y + PLAYER_HEIGHT + PLAYER_EYE_OFFSET;

    if (p->on_ground) {
        float dx = pos.x - p->cam.position.x;
        float dz = pos.z - p->cam.position.z;
        p->walk_distance += sqrtf(dx * dx + dz * dz);
    }

    p->cam.position = pos;
}

/* --- Scene-aware versions of collision helpers --- */

static float ground_check_scene(Vec3 head_pos, const Scene *scene) {
    Ray down = {vec3(head_pos.x, head_pos.y + STEP_HEIGHT + 0.1f, head_pos.z),
                vec3(0, -1, 0)};
    HitRecord rec = scene_intersect(scene, down, 0.0f, PLAYER_HEIGHT + STEP_HEIGHT + 1.0f);
    if (rec.hit && rec.normal.y > 0.5f)
        return rec.point.y;
    return -1000.0f;
}

static Vec3 try_move_axis_scene(Vec3 pos, Vec3 move, float foot_y, const Scene *scene) {
    float move_len = vec3_length(move);
    if (move_len < 1e-6f) return pos;

    Vec3 move_dir = vec3_mul(move, 1.0f / move_len);
    float check_dist = move_len + PLAYER_RADIUS;

    float check_heights[] = {foot_y + 0.2f, foot_y + PLAYER_HEIGHT * 0.5f, foot_y + PLAYER_HEIGHT - 0.1f};

    for (int i = 0; i < 3; i++) {
        Vec3 ray_origin = vec3(pos.x, check_heights[i], pos.z);
        Ray ray = {ray_origin, move_dir};
        HitRecord rec = scene_intersect(scene, ray, 0.0f, check_dist);
        if (rec.hit) {
            float allowed = rec.t - PLAYER_RADIUS;
            if (allowed < 0) allowed = 0;
            if (allowed < move_len)
                move_len = allowed;
        }
    }

    return vec3_add(pos, vec3_mul(move_dir, move_len));
}

void player_update_scene(Player *p, const PlayerCmd *cmd, float dt, Scene *scene) {
    apply_look(p, cmd);
    Vec3 horizontal_move = vec3_mul(wish_dir(p, cmd), PLAYER_SPEED * dt);

    /* Jump */
    if (cmd_has(cmd, BTN_JUMP) && p->on_ground) {
        p->velocity.y = PLAYER_JUMP_VEL;
        p->on_ground = false;
    }

    /* Gravity */
    if (!p->on_ground)
        p->velocity.y -= PLAYER_GRAVITY * dt;

    /* Horizontal movement with wall collision */
    Vec3 pos = p->cam.position;
    Vec3 move_x = vec3(horizontal_move.x, 0, 0);
    pos = try_move_axis_scene(pos, move_x, p->foot_y, scene);
    Vec3 move_z = vec3(0, 0, horizontal_move.z);
    pos = try_move_axis_scene(pos, move_z, p->foot_y, scene);

    /* Vertical velocity */
    p->foot_y += p->velocity.y * dt;

    /* Ground check */
    float ground_y = ground_check_scene(vec3(pos.x, p->foot_y + PLAYER_HEIGHT, pos.z), scene);
    if (p->foot_y <= ground_y) {
        p->foot_y = ground_y;
        if (p->velocity.y <= 0) {
            p->velocity.y = 0;
            p->on_ground = true;
        }
    } else if (p->foot_y > ground_y + 0.1f) {
        p->on_ground = false;
    }

    pos.y = p->foot_y + PLAYER_HEIGHT + PLAYER_EYE_OFFSET;

    /* Track horizontal distance walked (for footstep sounds) */
    if (p->on_ground) {
        float dx = pos.x - p->cam.position.x;
        float dz = pos.z - p->cam.position.z;
        p->walk_distance += sqrtf(dx * dx + dz * dz);
    }

    p->cam.position = pos;

    /* Flashlight toggle */
    if (cmd_has(cmd, BTN_FLASHLIGHT)) {
        p->flashlight_on = !p->flashlight_on;
    }

    /* Entity interaction: collect pickups */
    p->collected = entity_try_collect(&scene->entities, pos, COLLECT_RADIUS);

    /* Entity interaction: use doors */
    p->door_action = 0;
    if (cmd_has(cmd, BTN_USE)) {
        p->door_action = entity_try_use_door(&scene->entities, pos, yaw_forward(p), USE_RANGE);
    }
}
