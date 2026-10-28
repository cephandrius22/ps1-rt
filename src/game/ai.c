#include "ai.h"
#include "movement.h"
#include <math.h>
#include <stddef.h>

#define ENEMY_EYE_HEIGHT  1.5f  /* sight rays start here above the feet */
#define ENEMY_STEP        NAV_STEP /* the steps the nav grid links */
#define ENEMY_GRAVITY     18.0f
#define ENEMY_GROUND_SEARCH 1.0f
#define TRACE_EPS         0.001f
#define REPATH_TIME       0.5f  /* seconds between path searches while chasing */
#define WAYPOINT_REACHED  0.3f  /* meters: close enough, search the next one */
#define WALK_PROBE        (ENEMY_STEP + 0.05f) /* straight-line check above steps */

static const EnemyDef enemy_defs[ENTITY_TYPE_COUNT] = {
    [ENTITY_GHOUL] = {
        .speed       = 2.6f,
        .sight_range = 20.0f,
        .melee_range = 1.3f,
        .reach       = 1.8f,
        .damage      = 12.0f,
        .windup      = 0.4f,
        .cooldown    = 0.8f,
        .pain_time   = 0.15f,
        .radius      = 0.4f,
        .height      = 1.7f,
        .attack      = ATTACK_MELEE,
    },
    [ENTITY_SPITTER] = {
        .speed       = 2.0f,
        .sight_range = 22.0f,
        .melee_range = 0.0f,
        .windup      = 0.5f,
        .cooldown    = 1.6f,
        .pain_time   = 0.2f,
        .radius      = 0.35f,
        .height      = 1.4f,
        .attack      = ATTACK_PROJECTILE,
        .projectile  = PROJECTILE_SPIT,
        .preferred_range = 9.0f,
        .muzzle_height = 1.0f,
    },
};

const EnemyDef *enemy_def(EntityType type) {
    if (type <= ENTITY_NONE || type >= ENTITY_TYPE_COUNT) {
        return NULL;
    }
    const EnemyDef *def = &enemy_defs[type];
    return def->speed > 0 ? def : NULL;
}

static float flat_distance(Vec3 a, Vec3 b) {
    float dx = b.x - a.x;
    float dz = b.z - a.z;
    return sqrtf(dx * dx + dz * dz);
}

static void push_event(EventQueue *ev, GameEventType type, const Entity *e, int index) {
    events_push(ev, (GameEvent){.type = type, .pos = e->position, .entity = index});
}

/* Clear line from the enemy's eyes to the player's, within sight range */
static bool sees_player(const Entity *e, const EnemyDef *def, const Player *p,
                        const Scene *scene) {
    Vec3 eye = vec3_add(e->position, vec3(0, ENEMY_EYE_HEIGHT, 0));
    Vec3 to_player = vec3_sub(p->view.pos, eye);
    float dist = vec3_length(to_player);
    if (dist > def->sight_range) {
        return false;
    }

    Ray ray = {eye, vec3_mul(to_player, 1.0f / dist)};
    return !scene_trace(scene, ray, TRACE_EPS, dist, TRACE_SOLID).hit;
}

static BodyShape enemy_shape(const EnemyDef *def) {
    return (BodyShape){def->radius, def->height, ENEMY_STEP};
}

/* Can it walk straight at the target: same floor height (within a step)
 * and nothing solid between, just above step height */
static bool straight_line(const Entity *e, Vec3 target, const Scene *scene) {
    if (fabsf(target.y - e->position.y) > ENEMY_STEP) {
        return false;
    }
    Vec3 from = vec3_add(e->position, vec3(0, WALK_PROBE, 0));
    Vec3 to = vec3(target.x - e->position.x, 0, target.z - e->position.z);
    float dist = vec3_length(to);
    if (dist < 1e-4f) {
        return true;
    }
    Ray ray = {from, vec3_mul(to, 1.0f / dist)};
    return !scene_trace(scene, ray, TRACE_EPS, dist, TRACE_SOLID).hit;
}

/* Where to head for: the player when the way is straight, else the nav
 * path's next waypoint, searched again every REPATH_TIME or on arrival.
 * No path (no grid, unreachable): straight at the player, as before. */
static Vec3 steer_target(Entity *e, const Player *p, Scene *scene, float dt) {
    Vec3 feet = vec3(p->view.pos.x, p->foot_y, p->view.pos.z);
    if (straight_line(e, feet, scene)) {
        e->enemy.routed = false;
        return feet;
    }

    e->enemy.repath -= dt;
    bool arrived = e->enemy.routed &&
                   flat_distance(e->position, e->enemy.waypoint) < WAYPOINT_REACHED;
    if (!e->enemy.routed || arrived || e->enemy.repath <= 0) {
        e->enemy.routed = nav_next_waypoint(&scene->nav, e->position, feet, &e->enemy.waypoint);
        e->enemy.repath = REPATH_TIME;
    }
    return e->enemy.routed ? e->enemy.waypoint : feet;
}

/* Walk toward the steer target, sliding along walls axis by axis */
static void chase(Entity *e, const EnemyDef *def, const Player *p, Scene *scene, float dt) {
    Vec3 target = steer_target(e, p, scene, dt);
    Vec3 to = vec3(target.x - e->position.x, 0, target.z - e->position.z);
    float dist = vec3_length(to);
    if (dist < 1e-4f) {
        return;
    }

    float step = fminf(def->speed * dt, dist);
    Vec3 move = vec3_mul(to, step / dist);
    e->position = movement_slide(scene, e->position, move, enemy_shape(def));
}

/* Gravity and steps: stand on the ground under the feet (stepping up
 * ledges lower than ENEMY_STEP), or fall toward it */
static void settle(Entity *e, const Scene *scene, float dt) {
    float fall = e->enemy.fall_speed * dt;
    float ground = movement_ground(scene, e->position, ENEMY_STEP, ENEMY_GROUND_SEARCH + fall);

    if (e->position.y - fall > ground) {
        e->enemy.fall_speed += ENEMY_GRAVITY * dt;
        e->position.y -= fall;
        return;
    }
    e->position.y = ground;
    e->enemy.fall_speed = 0;
}

#define AIM_BELOW_EYE 0.4f /* ranged shots aim at the chest */

/* Ranged attack: spit at the player's chest. Fired last: spawning may grow
 * the entity list and move the shooter's Entity in memory. */
static void spit(Scene *scene, int index, const EnemyDef *def, const Player *p) {
    Vec3 from = vec3_add(scene->entities.items[index].position, vec3(0, def->muzzle_height, 0));
    Vec3 target = vec3_sub(p->view.pos, vec3(0, AIM_BELOW_EYE, 0));
    projectile_fire(scene, def->projectile, from, vec3_sub(target, from), index);
}

/* In range and in sight: the decision point for any attack */
static bool can_attack(const Entity *e, const EnemyDef *def, const Player *p,
                       const Scene *scene, float dist) {
    if (!player_alive(p) || e->enemy.cooldown > 0) {
        return false;
    }
    if (def->attack == ATTACK_MELEE) {
        return dist <= def->melee_range;
    }
    return dist <= def->preferred_range && sees_player(e, def, p, scene);
}

static void tick_enemy(Entity *e, int index, const EnemyDef *def, Player *p,
                       Scene *scene, float dt, EventQueue *ev) {
    e->enemy.state_time += dt;
    e->enemy.cooldown = fmaxf(e->enemy.cooldown - dt, 0.0f);
    float dist = flat_distance(e->position, p->view.pos);

    switch (e->enemy.state) {
    case ENEMY_DEAD:
        return;

    case ENEMY_IDLE:
        if (sees_player(e, def, p, scene)) {
            enemy_set_state(e, ENEMY_CHASE);
            push_event(ev, EV_ENEMY_ALERT, e, index);
        }
        return;

    case ENEMY_PAIN:
        if (e->enemy.state_time >= def->pain_time) {
            enemy_set_state(e, ENEMY_CHASE);
        }
        return;

    case ENEMY_CHASE: {
        if (can_attack(e, def, p, scene, dist)) {
            enemy_set_state(e, ENEMY_ATTACK);
            push_event(ev, EV_ENEMY_ATTACK, e, index);
            return;
        }
        /* Close in: melee until in reach; ranged until it has a clear
         * shot within its preferred range */
        bool in_position = def->attack == ATTACK_MELEE
                         ? dist <= def->melee_range
                         : dist <= def->preferred_range && sees_player(e, def, p, scene);
        if (!in_position) {
            chase(e, def, p, scene, dt);
        }
        return;
    }

    case ENEMY_ATTACK:
        if (e->enemy.state_time < def->windup) {
            return;
        }
        e->enemy.cooldown = def->cooldown;
        enemy_set_state(e, ENEMY_CHASE);

        if (def->attack == ATTACK_PROJECTILE) {
            /* No shooting through walls: lost sight during the windup */
            if (sees_player(e, def, p, scene)) {
                spit(scene, index, def, p);
            }
            return;
        }
        /* Strike lands only if the player didn't back out of reach */
        if (dist <= def->reach) {
            player_damage(p, def->damage, ev);
        }
        return;
    }
}

void ai_update(Scene *scene, Player *player, float dt, EventQueue *ev) {
    EntityList *list = &scene->entities;
    for (int i = 0; i < list->count; i++) {
        Entity *e = &list->items[i];
        const EnemyDef *def = enemy_def(e->type);
        if (!e->active || !def) {
            continue;
        }
        settle(e, scene, dt);
        tick_enemy(e, i, def, player, scene, dt, ev);
    }
}
