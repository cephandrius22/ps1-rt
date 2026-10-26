#include "player.h"
#include "scene.h"
#include <math.h>

#define PLAYER_TOUCH_MAX  8        /* entities touched per tick */
#define GROUND_NONE       -1000.0f /* ground_check() result: nothing below */
#define GROUND_MIN_NORMAL 0.5f     /* steeper surfaces are walls, not floor */
#define GROUND_SNAP       0.1f     /* airborne once this far above ground */
#define GROUND_PROBE_PAD  0.1f     /* extra lift on the ground probe origin */
#define WALL_PROBE_KNEE   0.2f     /* knee probe height above feet */
#define WALL_PROBE_HEAD   0.1f     /* head probe depth below top of body */

Player player_create(Vec3 pos) {
    Player p;
    p.foot_y = pos.y - PLAYER_HEIGHT - PLAYER_EYE_OFFSET;
    p.view = (Pose){.pos = pos, .yaw = 0.0f, .pitch = 0.0f};
    p.velocity = vec3(0, 0, 0);
    p.on_ground = false;
    p.flashlight_on = false;
    p.walk_distance = 0;
    return p;
}

/* --- Collision --- */

/* Height of walkable ground under head_pos, or GROUND_NONE. The ray
 * starts a step above the head so ledges up to STEP_HEIGHT are found. */
static float ground_check(Vec3 head_pos, const Scene *scene) {
    Ray down = {vec3(head_pos.x, head_pos.y + STEP_HEIGHT + GROUND_PROBE_PAD, head_pos.z),
                vec3(0, -1, 0)};
    HitRecord rec = scene_intersect(scene, down, 0.0f, PLAYER_HEIGHT + STEP_HEIGHT + 1.0f);
    if (!rec.hit || rec.normal.y <= GROUND_MIN_NORMAL) {
        return GROUND_NONE;
    }
    return rec.point.y;
}

/* Move along one axis, stopping PLAYER_RADIUS short of walls. Probes at
 * knee, waist and head so low and overhanging obstacles both block.
 *
 *   head  ──>|
 *   waist ──>|  wall
 *   knee  ──>|
 */
static Vec3 try_move_axis(Vec3 pos, Vec3 move, float foot_y, const Scene *scene) {
    float move_len = vec3_length(move);
    if (move_len < 1e-6f) {
        return pos;
    }

    Vec3 move_dir = vec3_mul(move, 1.0f / move_len);
    float check_dist = move_len + PLAYER_RADIUS;
    float probe_heights[] = {
        foot_y + WALL_PROBE_KNEE,
        foot_y + PLAYER_HEIGHT * 0.5f,
        foot_y + PLAYER_HEIGHT - WALL_PROBE_HEAD,
    };

    for (int i = 0; i < 3; i++) {
        Ray ray = {vec3(pos.x, probe_heights[i], pos.z), move_dir};
        HitRecord rec = scene_intersect(scene, ray, 0.0f, check_dist);
        if (!rec.hit) {
            continue;
        }
        float allowed = fmaxf(rec.t - PLAYER_RADIUS, 0.0f);
        move_len = fminf(move_len, allowed);
    }

    return vec3_add(pos, vec3_mul(move_dir, move_len));
}

/* --- Movement --- */

/* Apply look deltas, clamping pitch so the view can't flip over */
static void apply_look(Player *p, const PlayerCmd *cmd) {
    p->view.yaw += cmd->yaw_delta;
    p->view.pitch = fmaxf(-PLAYER_PITCH_MAX,
                         fminf(PLAYER_PITCH_MAX, p->view.pitch + cmd->pitch_delta));
}

/* Horizontal forward direction for the current yaw (yaw=0 → -Z) */
static Vec3 yaw_forward(const Player *p) {
    return vec3(-sinf(p->view.yaw), 0, -cosf(p->view.yaw));
}

/* Yaw-relative wish direction. Length capped at 1 so diagonals aren't
 * faster, while partial (analog) input still moves slower. */
static Vec3 wish_dir(const Player *p, const PlayerCmd *cmd) {
    Vec3 forward = yaw_forward(p);
    Vec3 right = vec3(cosf(p->view.yaw), 0, -sinf(p->view.yaw));
    Vec3 dir = vec3_add(vec3_mul(forward, cmd->forward), vec3_mul(right, cmd->strafe));

    if (vec3_dot(dir, dir) > 1.0f) {
        dir = vec3_normalize(dir);
    }
    return dir;
}

/* Integrate gravity and snap to ground; updates foot_y and on_ground */
static void apply_vertical(Player *p, Vec3 pos, float dt, const Scene *scene) {
    p->foot_y += p->velocity.y * dt;

    float ground_y = ground_check(vec3(pos.x, p->foot_y + PLAYER_HEIGHT, pos.z), scene);
    if (p->foot_y > ground_y + GROUND_SNAP) {
        p->on_ground = false;
        return;
    }
    if (p->foot_y > ground_y) {
        return;
    }

    p->foot_y = ground_y;
    if (p->velocity.y <= 0) {
        p->velocity.y = 0;
        p->on_ground = true;
    }
}

/* Accumulate ground distance; emit one footstep per PLAYER_STEP_DIST */
static void track_footsteps(Player *p, Vec3 new_pos, EventQueue *ev) {
    if (!p->on_ground) {
        return;
    }

    float dx = new_pos.x - p->view.pos.x;
    float dz = new_pos.z - p->view.pos.z;
    p->walk_distance += sqrtf(dx * dx + dz * dz);

    if (p->walk_distance < PLAYER_STEP_DIST) {
        return;
    }
    p->walk_distance -= PLAYER_STEP_DIST;

    Vec3 feet = vec3(new_pos.x, p->foot_y, new_pos.z);
    events_push(ev, (GameEvent){.type = EV_FOOTSTEP, .pos = feet,
                                .entity = ENTITY_INDEX_NONE});
}

/* --- Interaction --- */

/* Walk-over pickups: anything inside the body cylinder */
static void touch_entities(const Player *p, Scene *scene, EventQueue *ev) {
    Vec3 pos = p->view.pos;
    Cylinder body = {.base = vec3(pos.x, p->foot_y, pos.z),
                     .radius = COLLECT_RADIUS, .height = PLAYER_HEIGHT};

    int touched[PLAYER_TOUCH_MAX];
    int n = entity_touch(&scene->entities, body, touched, PLAYER_TOUCH_MAX);
    for (int i = 0; i < n; i++) {
        const Entity *e = &scene->entities.items[touched[i]];
        events_push(ev, (GameEvent){.type = EV_PICKUP, .pos = pos,
                                    .entity = touched[i], .amount = e->pickup.amount});
    }
}

/* Use key: nearest usable entity in front (doors) */
static void use_entity(const Player *p, Scene *scene, EventQueue *ev) {
    Vec3 pos = p->view.pos;
    int used;
    UseResult res = entity_try_use(&scene->entities, pos, yaw_forward(p), USE_RANGE, &used);
    if (res == USE_NOTHING) {
        return;
    }

    GameEventType type = (res == USE_OPENED) ? EV_DOOR_OPENED : EV_DOOR_CLOSED;
    events_push(ev, (GameEvent){.type = type, .pos = pos, .entity = used});
}

/* --- Tick --- */

void player_update(Player *p, const PlayerCmd *cmd, float dt, Scene *scene,
                   EventQueue *ev) {
    apply_look(p, cmd);

    if (cmd_has(cmd, BTN_JUMP) && p->on_ground) {
        p->velocity.y = PLAYER_JUMP_VEL;
        p->on_ground = false;
    }
    if (!p->on_ground) {
        p->velocity.y -= PLAYER_GRAVITY * dt;
    }

    /* Slide along walls: resolve X and Z independently */
    Vec3 move = vec3_mul(wish_dir(p, cmd), PLAYER_SPEED * dt);
    Vec3 pos = p->view.pos;
    pos = try_move_axis(pos, vec3(move.x, 0, 0), p->foot_y, scene);
    pos = try_move_axis(pos, vec3(0, 0, move.z), p->foot_y, scene);

    apply_vertical(p, pos, dt, scene);
    pos.y = p->foot_y + PLAYER_HEIGHT + PLAYER_EYE_OFFSET;

    track_footsteps(p, pos, ev);
    p->view.pos = pos;

    if (cmd_has(cmd, BTN_FLASHLIGHT)) {
        p->flashlight_on = !p->flashlight_on;
    }

    touch_entities(p, scene, ev);
    if (cmd_has(cmd, BTN_USE)) {
        use_entity(p, scene, ev);
    }
}
