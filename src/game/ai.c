#include "ai.h"
#include <math.h>
#include <stddef.h>

#define ENEMY_EYE_HEIGHT  1.5f  /* sight rays start here above the feet */
#define ENEMY_PROBE_KNEE  0.3f  /* wall probe heights above the feet */
#define ENEMY_PROBE_WAIST 0.9f
#define TRACE_EPS         0.001f

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

/* Move along one axis, stopping radius short of anything solid */
static Vec3 step_axis(Vec3 pos, Vec3 move, float radius, const Scene *scene) {
    float len = vec3_length(move);
    if (len < 1e-6f) {
        return pos;
    }

    Vec3 dir = vec3_mul(move, 1.0f / len);
    float heights[] = {ENEMY_PROBE_KNEE, ENEMY_PROBE_WAIST};
    for (int i = 0; i < 2; i++) {
        Ray ray = {vec3_add(pos, vec3(0, heights[i], 0)), dir};
        HitRecord hit = scene_trace(scene, ray, TRACE_EPS, len + radius, TRACE_SOLID);
        if (hit.hit) {
            len = fminf(len, fmaxf(hit.t - radius, 0.0f));
        }
    }
    return vec3_add(pos, vec3_mul(dir, len));
}

/* Walk straight at the player, sliding along walls axis by axis */
static void chase(Entity *e, const EnemyDef *def, const Player *p,
                  const Scene *scene, float dt) {
    Vec3 to = vec3(p->view.pos.x - e->position.x, 0, p->view.pos.z - e->position.z);
    float dist = vec3_length(to);
    if (dist < 1e-4f) {
        return;
    }

    Vec3 move = vec3_mul(to, def->speed * dt / dist);
    Vec3 pos = e->position;
    pos = step_axis(pos, vec3(move.x, 0, 0), def->radius, scene);
    pos = step_axis(pos, vec3(0, 0, move.z), def->radius, scene);
    e->position = pos;
}

static void tick_enemy(Entity *e, int index, const EnemyDef *def, Player *p,
                       const Scene *scene, float dt, EventQueue *ev) {
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

    case ENEMY_CHASE:
        if (dist <= def->melee_range && e->enemy.cooldown <= 0 && player_alive(p)) {
            enemy_set_state(e, ENEMY_ATTACK);
            push_event(ev, EV_ENEMY_ATTACK, e, index);
            return;
        }
        if (dist > def->melee_range) {
            chase(e, def, p, scene, dt);
        }
        return;

    case ENEMY_ATTACK:
        if (e->enemy.state_time < def->windup) {
            return;
        }
        /* Strike lands only if the player didn't back out of reach */
        if (dist <= def->reach) {
            player_damage(p, def->damage, ev);
        }
        e->enemy.cooldown = def->cooldown;
        enemy_set_state(e, ENEMY_CHASE);
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
        tick_enemy(e, i, def, player, scene, dt, ev);
    }
}
