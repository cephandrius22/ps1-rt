/*
 * event.h — Queue of things that happened in the game this frame.
 *
 * Gameplay pushes events; presentation (audio, later HUD/particles)
 * drains them. Producers never call consumers directly, so adding a
 * reaction to an event never touches gameplay code.
 *
 *   player / weapon ──push──> EventQueue ──drain──> main → audio
 *                                                  (later: hud, fx)
 *
 * Cleared at the start of each game_update(), so one frame's events
 * from all of its ticks reach presentation. Overflow drops the newest.
 */
#ifndef EVENT_H
#define EVENT_H

#include "math3d.h"
#include "entity.h" /* ENTITY_INDEX_NONE */

#define EVENT_QUEUE_MAX 64

typedef enum {
    EV_WEAPON_FIRED,  /* pos = muzzle */
    EV_WEAPON_IMPACT, /* pos = hit point, entity = hit or none, amount = damage */
    EV_WEAPON_EMPTY,  /* pos = muzzle; attack pressed without ammo */
    EV_FOOTSTEP,      /* pos = feet */
    EV_PICKUP,        /* pos = collector, entity = pickup, amount = value */
    EV_DOOR_OPENED,   /* pos = user, entity = door */
    EV_DOOR_CLOSED,   /* pos = user, entity = door */
    EV_PLAYER_HURT,   /* pos = player eye, amount = damage taken */
    EV_PLAYER_DIED,   /* pos = player eye, amount = final blow */
    EV_ENEMY_ALERT,   /* pos = enemy feet, entity = enemy; spotted the player */
    EV_ENEMY_ATTACK,  /* pos = enemy feet, entity = enemy; strike wind-up */
    EV_ENEMY_PAIN,    /* pos = enemy feet, entity = enemy, amount = damage */
    EV_ENEMY_DIED,    /* pos = enemy feet, entity = enemy */
} GameEventType;

typedef struct {
    GameEventType type;
    Vec3 pos;
    int entity;   /* entity index, or ENTITY_INDEX_NONE */
    float amount;
} GameEvent;

typedef struct {
    GameEvent items[EVENT_QUEUE_MAX];
    int count;
} EventQueue;

void events_clear(EventQueue *q);
void events_push(EventQueue *q, GameEvent ev);

#endif
