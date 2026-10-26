#include "player.h"
#include "scene.h"
#include <math.h>

#define PLAYER_TOUCH_MAX 8 /* entities touched per tick */

Player player_create(Vec3 pos) {
    Player p;
    p.foot_y = pos.y - PLAYER_HEIGHT - PLAYER_EYE_OFFSET;
    p.cam = camera_create(pos, 0.0f, 0.0f, 1.2f);
    p.velocity = vec3(0, 0, 0);
    p.on_ground = false;
    p.flashlight_on = false;
    p.walk_distance = 0;
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

/* Accumulate ground distance; emit one footstep per PLAYER_STEP_DIST */
static void track_footsteps(Player *p, Vec3 new_pos, EventQueue *ev) {
    if (!p->on_ground) {
        return;
    }

    float dx = new_pos.x - p->cam.position.x;
    float dz = new_pos.z - p->cam.position.z;
    p->walk_distance += sqrtf(dx * dx + dz * dz);

    if (p->walk_distance < PLAYER_STEP_DIST) {
        return;
    }
    p->walk_distance -= PLAYER_STEP_DIST;

    Vec3 feet = vec3(new_pos.x, p->foot_y, new_pos.z);
    events_push(ev, (GameEvent){.type = EV_FOOTSTEP, .pos = feet,
                                .entity = ENTITY_INDEX_NONE});
}

void player_update(Player *p, const PlayerCmd *cmd, float dt, const BVH *world,
                   EventQueue *ev) {
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

    track_footsteps(p, pos, ev);

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

void player_update_scene(Player *p, const PlayerCmd *cmd, float dt, Scene *scene,
                         EventQueue *ev) {
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

    track_footsteps(p, pos, ev);

    p->cam.position = pos;

    /* Flashlight toggle */
    if (cmd_has(cmd, BTN_FLASHLIGHT)) {
        p->flashlight_on = !p->flashlight_on;
    }

    /* Entity interaction: walk-over pickups */
    Cylinder body = {.base = vec3(pos.x, p->foot_y, pos.z),
                     .radius = COLLECT_RADIUS, .height = PLAYER_HEIGHT};
    int touched[PLAYER_TOUCH_MAX];
    int n = entity_touch(&scene->entities, body, touched, PLAYER_TOUCH_MAX);
    for (int i = 0; i < n; i++) {
        const Entity *e = &scene->entities.items[touched[i]];
        events_push(ev, (GameEvent){.type = EV_PICKUP, .pos = pos,
                                    .entity = touched[i], .amount = e->pickup.amount});
    }

    /* Entity interaction: use (doors) */
    if (!cmd_has(cmd, BTN_USE)) {
        return;
    }
    int used;
    UseResult res = entity_try_use(&scene->entities, pos, yaw_forward(p), USE_RANGE, &used);
    if (res == USE_NOTHING) {
        return;
    }
    GameEventType type = (res == USE_OPENED) ? EV_DOOR_OPENED : EV_DOOR_CLOSED;
    events_push(ev, (GameEvent){.type = type, .pos = pos, .entity = used});
}
