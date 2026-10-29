#include "logic.h"
#include "ai.h"
#include <math.h>
#include <string.h>

#define MONSTER_DOOR_REACH 0.5f /* monsters open doors this close to their body */

static bool is_door(const Entity *e) {
    return e->active && e->type == ENTITY_DOOR;
}

static bool is_button(const Entity *e) {
    return e->active && e->type == ENTITY_BUTTON;
}

static bool is_trigger(const Entity *e) {
    return e->active && (e->type == ENTITY_TRIGGER_ONCE || e->type == ENTITY_TRIGGER_MULTIPLE);
}

/* Locked: needs a keycard, or only a trigger may open it */
static bool door_locked(const Entity *e) {
    return e->mover.key[0] != '\0' || e->targetname[0] != '\0';
}

static void push(EventQueue *ev, GameEventType type, int i, const Entity *e) {
    events_push(ev, (GameEvent){.type = type, .pos = e->position, .entity = i});
}

/* --- Bodies touching a brush --- */

static bool boxes_overlap(Vec3 alo, Vec3 ahi, Vec3 blo, Vec3 bhi) {
    return alo.x < bhi.x && ahi.x > blo.x && alo.y < bhi.y && ahi.y > blo.y &&
           alo.z < bhi.z && ahi.z > blo.z;
}

/* Upright body as a box, grown by pad on every side */
static bool body_touches(const Entity *brush, Vec3 feet, float radius, float height,
                         float pad) {
    Vec3 lo, hi;
    entity_bounds(brush, &lo, &hi);
    float r = radius + pad;
    Vec3 blo = vec3(feet.x - r, feet.y - pad, feet.z - r);
    Vec3 bhi = vec3(feet.x + r, feet.y + height + pad, feet.z + r);
    return boxes_overlap(lo, hi, blo, bhi);
}

static bool player_touches(const Entity *brush, const Player *p, float pad) {
    Vec3 feet = vec3(p->view.pos.x, p->foot_y, p->view.pos.z);
    return player_alive(p) && body_touches(brush, feet, PLAYER_RADIUS, PLAYER_HEIGHT, pad);
}

/* A living monster whose body (grown by pad) touches the brush; with
 * CHASERS only those hunting the player count */
typedef enum { ANY_MONSTER, CHASERS } MonsterFilter;

static bool monster_touches(const Scene *s, const Entity *brush, float pad,
                            MonsterFilter filter) {
    const EntityList *list = &s->entities;
    for (int i = 0; i < list->count; i++) {
        const Entity *e = &list->items[i];
        const EnemyDef *def = enemy_def(e->type);
        if (!e->active || !def || e->enemy.state == ENEMY_DEAD) {
            continue;
        }
        if (filter == CHASERS && e->enemy.state == ENEMY_IDLE) {
            continue;
        }
        if (body_touches(brush, e->position, def->radius, def->height, pad)) {
            return true;
        }
    }
    return false;
}

/* --- Movers: the slide shared by doors and buttons --- */

/* OPENING → OPEN at full travel, CLOSING → CLOSED at rest */
static void mover_slide(Entity *e, float dt) {
    float step = e->mover.speed * dt;
    if (e->mover.state == MOVER_OPENING) {
        e->mover.offset = fminf(e->mover.offset + step, e->mover.travel);
        if (e->mover.offset >= e->mover.travel) {
            e->mover.state = MOVER_OPEN;
            e->mover.timer = 0;
        }
    } else if (e->mover.state == MOVER_CLOSING) {
        e->mover.offset = fmaxf(e->mover.offset - step, 0.0f);
        if (e->mover.offset <= 0) {
            e->mover.state = MOVER_CLOSED;
        }
    }
}

/* Count time spent open; true once `wait` is up (never if wait < 0) */
static bool mover_wait_over(Entity *e, float dt) {
    if (e->mover.state != MOVER_OPEN) {
        return false;
    }
    e->mover.timer += dt;
    return e->mover.wait >= 0 && e->mover.timer >= e->mover.wait;
}

static bool has_keycard_for(const Entity *e, const Player *p) {
    return e->mover.key[0] == '\0' ||
           inventory_has_keycard(&p->inv, keycard_by_name(e->mover.key));
}

/* --- Doors --- */

static void door_open(Entity *e, int i, EventQueue *ev) {
    if (e->mover.state == MOVER_OPENING) {
        return;
    }
    if (e->mover.state == MOVER_OPEN) {
        e->mover.timer = 0; /* someone's still going through */
        return;
    }
    e->mover.state = MOVER_OPENING;
    push(ev, EV_DOOR_OPENED, i, e);
}

static void door_close(Entity *e, int i, EventQueue *ev) {
    e->mover.state = MOVER_CLOSING;
    push(ev, EV_DOOR_CLOSED, i, e);
}

/* Closed doors mark the nav grid; opening ones unmark it */
static void sync_nav(Scene *s, Entity *e) {
    bool shut = e->mover.state == MOVER_CLOSED;
    if (shut == e->mover.nav_closed) {
        return;
    }
    Vec3 lo, hi;
    entity_bounds(e, &lo, &hi);
    NavDoor kind = door_locked(e) ? NAV_DOOR_LOCKED : NAV_DOOR_OPENABLE;
    nav_set_door(&s->nav, lo, hi, kind, shut ? 1 : -1);
    e->mover.nav_closed = shut;
}

static void door_tick(Scene *s, int i, const Player *p, float dt, EventQueue *ev) {
    Entity *e = &s->entities.items[i];

    /* Doom: monsters open plain doors they walk into */
    bool closed = e->mover.state == MOVER_CLOSED;
    if (closed && !door_locked(e) && monster_touches(s, e, MONSTER_DOOR_REACH, CHASERS)) {
        door_open(e, i, ev);
    }

    if (mover_wait_over(e, dt)) {
        door_close(e, i, ev);
    }

    /* Never crush: anything in the way sends it back up */
    bool closing = e->mover.state == MOVER_CLOSING;
    if (closing && (player_touches(e, p, 0) || monster_touches(s, e, 0, ANY_MONSTER))) {
        door_open(e, i, ev);
    }

    mover_slide(e, dt);
    sync_nav(s, e);
}

/* Doom: use on an open door shuts it early */
static void use_door(Entity *e, int i, EventQueue *ev) {
    if (e->mover.state == MOVER_OPEN) {
        door_close(e, i, ev);
        return;
    }
    door_open(e, i, ev);
}

/* --- Firing targets --- */

/* What a named entity does when its name is fired */
static void react(Entity *e, int i, EventQueue *ev) {
    if (is_door(e)) {
        door_open(e, i, ev);
        return;
    }

    /* Hidden monster: appears, already hunting */
    if (enemy_def(e->type) && !e->active && e->enemy.state != ENEMY_DEAD) {
        e->active = true;
        enemy_set_state(e, ENEMY_CHASE);
        push(ev, EV_ENEMY_ALERT, i, e);
    }
}

void logic_fire(Scene *s, const char *name, EventQueue *ev) {
    if (name[0] == '\0') {
        return;
    }
    for (int i = 0; i < s->entities.count; i++) {
        Entity *e = &s->entities.items[i];
        if (strcmp(e->targetname, name) == 0) {
            react(e, i, ev);
        }
    }
    for (int i = 0; i < s->lights.count; i++) {
        Light *light = &s->lights.items[i];
        if (strcmp(light->targetname, name) == 0) {
            light->off = !light->off;
        }
    }
}

/* A trigger or button went off: announce it (messages), fire its target */
static void fire_from(Scene *s, int i, EventQueue *ev) {
    const Entity *e = &s->entities.items[i];
    push(ev, EV_TRIGGERED, i, e);
    logic_fire(s, e->target, ev);
}

/* --- Buttons --- */

static void press(Scene *s, int i, EventQueue *ev) {
    Entity *e = &s->entities.items[i];
    if (e->mover.state != MOVER_CLOSED) {
        return;
    }
    e->mover.state = MOVER_OPENING;
    push(ev, EV_BUTTON_PRESSED, i, e);
    fire_from(s, i, ev);
}

/* Pops back out once its wait is up */
static void button_tick(Entity *e, float dt) {
    if (mover_wait_over(e, dt)) {
        e->mover.state = MOVER_CLOSING;
    }
    mover_slide(e, dt);
}

/* --- Triggers --- */

static void trigger_tick(Scene *s, int i, const Player *p, float dt, EventQueue *ev) {
    Entity *e = &s->entities.items[i];
    e->trigger.cooldown = fmaxf(e->trigger.cooldown - dt, 0.0f);
    if (e->trigger.cooldown > 0 || !player_touches(e, p, 0)) {
        return;
    }

    fire_from(s, i, ev);
    if (e->trigger.wait < 0) {
        e->active = false; /* trigger_once */
        return;
    }
    e->trigger.cooldown = e->trigger.wait;
}

/* --- Public --- */

void logic_update(Scene *s, const Player *p, float dt, EventQueue *ev) {
    for (int i = 0; i < s->entities.count; i++) {
        Entity *e = &s->entities.items[i];
        if (is_door(e)) {
            door_tick(s, i, p, dt, ev);
        } else if (is_button(e)) {
            button_tick(e, dt);
        } else if (is_trigger(e)) {
            trigger_tick(s, i, p, dt, ev);
        }
    }
}

void logic_use(Scene *s, const Player *p, EventQueue *ev) {
    Vec3 forward = vec3(-sinf(p->view.yaw), 0, -cosf(p->view.yaw));
    int i = entity_find_usable(&s->entities, p->view.pos, forward, USE_RANGE);
    if (i == ENTITY_INDEX_NONE) {
        return;
    }

    Entity *e = &s->entities.items[i];
    bool door = is_door(e);
    if (!door && !is_button(e)) {
        return;
    }
    if (door && e->targetname[0] != '\0') {
        return; /* waits for its trigger */
    }
    if (!has_keycard_for(e, p)) {
        push(ev, EV_LOCKED, i, e);
        return;
    }

    if (door) {
        use_door(e, i, ev);
        return;
    }
    press(s, i, ev);
}
