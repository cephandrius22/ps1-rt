#include "logic.h"
#include "ai.h"
#include <math.h>

#define MONSTER_DOOR_REACH 0.5f /* monsters open doors this close to their body */

static bool is_door(const Entity *e) {
    return e->active && e->type == ENTITY_DOOR;
}

/* Locked: needs a keycard, or only a trigger may open it */
static bool door_locked(const Entity *e) {
    return e->mover.key[0] != '\0' || e->targetname[0] != '\0';
}

static void push_door(EventQueue *ev, GameEventType type, int i, const Entity *e) {
    events_push(ev, (GameEvent){.type = type, .pos = e->position, .entity = i});
}

static void door_open(Entity *e, int i, EventQueue *ev) {
    if (e->mover.state == MOVER_OPENING) {
        return;
    }
    if (e->mover.state == MOVER_OPEN) {
        e->mover.timer = 0; /* someone's still going through */
        return;
    }
    e->mover.state = MOVER_OPENING;
    push_door(ev, EV_DOOR_OPENED, i, e);
}

static void door_close(Entity *e, int i, EventQueue *ev) {
    e->mover.state = MOVER_CLOSING;
    push_door(ev, EV_DOOR_CLOSED, i, e);
}

/* --- Bodies in a door's way --- */

static bool boxes_overlap(Vec3 alo, Vec3 ahi, Vec3 blo, Vec3 bhi) {
    return alo.x < bhi.x && ahi.x > blo.x && alo.y < bhi.y && ahi.y > blo.y &&
           alo.z < bhi.z && ahi.z > blo.z;
}

/* Upright body as a box, grown by pad on every side */
static bool body_touches(const Entity *door, Vec3 feet, float radius, float height,
                         float pad) {
    Vec3 lo, hi;
    entity_bounds(door, &lo, &hi);
    float r = radius + pad;
    Vec3 blo = vec3(feet.x - r, feet.y - pad, feet.z - r);
    Vec3 bhi = vec3(feet.x + r, feet.y + height + pad, feet.z + r);
    return boxes_overlap(lo, hi, blo, bhi);
}

static bool player_touches(const Entity *door, const Player *p, float pad) {
    Vec3 feet = vec3(p->view.pos.x, p->foot_y, p->view.pos.z);
    return player_alive(p) && body_touches(door, feet, PLAYER_RADIUS, PLAYER_HEIGHT, pad);
}

/* A living monster whose body (grown by pad) touches the door; with
 * CHASERS only those hunting the player count */
typedef enum { ANY_MONSTER, CHASERS } MonsterFilter;

static bool monster_touches(const Scene *s, const Entity *door, float pad,
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
        if (body_touches(door, e->position, def->radius, def->height, pad)) {
            return true;
        }
    }
    return false;
}

/* --- Per tick --- */

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
    float step = e->mover.speed * dt;

    switch (e->mover.state) {
    case MOVER_CLOSED:
        /* Doom: monsters open plain doors they walk into */
        if (!door_locked(e) && monster_touches(s, e, MONSTER_DOOR_REACH, CHASERS)) {
            door_open(e, i, ev);
        }
        break;

    case MOVER_OPENING:
        e->mover.offset = fminf(e->mover.offset + step, e->mover.travel);
        if (e->mover.offset >= e->mover.travel) {
            e->mover.state = MOVER_OPEN;
            e->mover.timer = 0;
        }
        break;

    case MOVER_OPEN:
        e->mover.timer += dt;
        if (e->mover.wait >= 0 && e->mover.timer >= e->mover.wait) {
            door_close(e, i, ev);
        }
        break;

    case MOVER_CLOSING:
        if (player_touches(e, p, 0) || monster_touches(s, e, 0, ANY_MONSTER)) {
            door_open(e, i, ev);
            break;
        }
        e->mover.offset = fmaxf(e->mover.offset - step, 0.0f);
        if (e->mover.offset <= 0) {
            e->mover.state = MOVER_CLOSED;
        }
        break;
    }
    sync_nav(s, e);
}

void logic_update(Scene *s, const Player *p, float dt, EventQueue *ev) {
    for (int i = 0; i < s->entities.count; i++) {
        if (is_door(&s->entities.items[i])) {
            door_tick(s, i, p, dt, ev);
        }
    }
}

/* --- Use --- */

/* Keyed doors open only for a holder of the card; named ones only
 * for their trigger */
static bool may_open(const Entity *door, const Player *p) {
    if (door->targetname[0] != '\0') {
        return false;
    }
    if (door->mover.key[0] == '\0') {
        return true;
    }
    return inventory_has_keycard(&p->inv, keycard_by_name(door->mover.key));
}

void logic_use(Scene *s, const Player *p, EventQueue *ev) {
    Vec3 forward = vec3(-sinf(p->view.yaw), 0, -cosf(p->view.yaw));
    int i = entity_find_usable(&s->entities, p->view.pos, forward, USE_RANGE);
    if (i == ENTITY_INDEX_NONE) {
        return;
    }

    Entity *e = &s->entities.items[i];
    if (!is_door(e)) {
        return;
    }
    if (!may_open(e, p)) {
        if (e->mover.key[0] != '\0') {
            push_door(ev, EV_DOOR_LOCKED, i, e);
        }
        return;
    }

    /* Doom: use on an open door shuts it early */
    if (e->mover.state == MOVER_OPEN) {
        door_close(e, i, ev);
        return;
    }
    door_open(e, i, ev);
}
