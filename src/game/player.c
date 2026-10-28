#include "player.h"
#include "scene.h"
#include "weapon.h"
#include "movement.h"
#include <math.h>

#define PLAYER_TOUCH_MAX  8        /* entities touched per tick */
#define GROUND_SNAP       0.1f     /* airborne once this far above ground */

Player player_create(Vec3 pos) {
    Player p;
    p.foot_y = pos.y - PLAYER_HEIGHT - PLAYER_EYE_OFFSET;
    p.view = (Pose){.pos = pos, .yaw = 0.0f, .pitch = 0.0f};
    p.velocity = vec3(0, 0, 0);
    p.on_ground = false;
    p.flashlight_on = false;
    p.walk_distance = 0;
    p.health = PLAYER_MAX_HEALTH;
    p.inv = inventory_create();
    return p;
}

/* --- Collision --- */

static const BodyShape player_shape = {PLAYER_RADIUS, PLAYER_HEIGHT, STEP_HEIGHT};

#define GROUND_SEARCH 1.0f /* look this far below the feet for ground, plus the fall */

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
    /* Search from where the feet were, so a fast fall can't skip a floor */
    float fall = p->velocity.y * dt;
    Vec3 feet = vec3(pos.x, p->foot_y, pos.z);
    float ground_y = movement_ground(scene, feet, STEP_HEIGHT, GROUND_SEARCH + fabsf(fall));
    p->foot_y += fall;

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

/* Weapon pickup: give the weapon and its ammo. Left in place only if
 * the weapon is already owned and its ammo is full. EV_WEAPON_PICKUP's
 * amount is 1 for a weapon the player didn't have (game switches to it). */
static bool take_weapon(Player *p, const Entity *e, int i, EventQueue *ev) {
    WeaponType type = weapon_type_by_name(e->pickup.item);
    if (type == WEAPON_TYPE_COUNT) {
        return false;
    }

    AmmoType ammo = weapon_def(type)->ammo;
    bool owned = inventory_has_weapon(&p->inv, type);
    if (owned && inventory_full(&p->inv, ammo)) {
        return false;
    }

    inventory_give_weapon(&p->inv, type);
    inventory_add(&p->inv, ammo, (int)e->pickup.amount);
    events_push(ev, (GameEvent){.type = EV_WEAPON_PICKUP, .pos = p->view.pos,
                                .entity = i, .amount = owned ? 0.0f : 1.0f});
    return true;
}

/* Apply what touching entity i means to the player. Returns false to
 * leave it untouched (e.g. health pickup at full health stays put). */
static bool apply_touch_effect(Player *p, const Entity *e, int i, EventQueue *ev) {
    switch (entity_def(e->type)->touch_effect) {
    case TOUCH_EFFECT_HEALTH:
        if (p->health >= PLAYER_MAX_HEALTH) {
            return false;
        }
        p->health = fminf(p->health + e->pickup.amount, PLAYER_MAX_HEALTH);
        return true;
    case TOUCH_EFFECT_AMMO: {
        AmmoType type = inventory_ammo_by_name(e->pickup.item);
        return inventory_add(&p->inv, type, (int)e->pickup.amount) > 0;
    }
    case TOUCH_EFFECT_WEAPON:
        return take_weapon(p, e, i, ev);
    case TOUCH_EFFECT_EXIT:
        events_push(ev, (GameEvent){.type = EV_LEVEL_EXIT, .pos = p->view.pos, .entity = i});
        return false;
    case TOUCH_EFFECT_KEYCARD: {
        Keycard card = keycard_by_name(e->pickup.item);
        if (inventory_has_keycard(&p->inv, card)) {
            return false;
        }
        inventory_give_keycard(&p->inv, card);
        return true;
    }
    case TOUCH_EFFECT_NONE:
        return true;
    }
    return true;
}

/* Walk-over pickups: anything inside the body cylinder */
static void touch_entities(Player *p, Scene *scene, EventQueue *ev) {
    Vec3 pos = p->view.pos;
    Cylinder body = {.base = vec3(pos.x, p->foot_y, pos.z),
                     .radius = COLLECT_RADIUS, .height = PLAYER_HEIGHT};

    int found[PLAYER_TOUCH_MAX];
    int n = entity_overlap(&scene->entities, body, found, PLAYER_TOUCH_MAX);
    for (int i = 0; i < n; i++) {
        const Entity *e = &scene->entities.items[found[i]];
        if (!apply_touch_effect(p, e, found[i], ev)) {
            continue;
        }
        if (entity_touch(&scene->entities, found[i]) != TOUCH_CONSUMED) {
            continue;
        }
        events_push(ev, (GameEvent){.type = EV_PICKUP, .pos = pos,
                                    .entity = found[i], .amount = e->pickup.amount});
    }
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
    Vec3 feet = movement_slide(scene, vec3(p->view.pos.x, p->foot_y, p->view.pos.z),
                               move, player_shape);
    Vec3 pos = vec3(feet.x, p->view.pos.y, feet.z);

    apply_vertical(p, pos, dt, scene);
    pos.y = p->foot_y + PLAYER_HEIGHT + PLAYER_EYE_OFFSET;

    float dx = pos.x - p->view.pos.x;
    float dz = pos.z - p->view.pos.z;
    p->move_speed = dt > 0 ? sqrtf(dx * dx + dz * dz) / dt : 0;

    track_footsteps(p, pos, ev);
    p->view.pos = pos;

    if (cmd_has(cmd, BTN_FLASHLIGHT)) {
        p->flashlight_on = !p->flashlight_on;
    }

    touch_entities(p, scene, ev);
}

/* --- Damage --- */

void player_damage(Player *p, float amount, EventQueue *ev) {
    if (!player_alive(p) || amount <= 0) {
        return;
    }

    p->health = fmaxf(p->health - amount, 0.0f);
    GameEventType type = player_alive(p) ? EV_PLAYER_HURT : EV_PLAYER_DIED;
    events_push(ev, (GameEvent){.type = type, .pos = p->view.pos,
                                .entity = ENTITY_INDEX_NONE, .amount = amount});
}
