/*
 * event.h — Per-tick queue of things that happened in the game.
 *
 * Gameplay pushes events; presentation (audio, later HUD/particles)
 * drains them. Producers never call consumers directly, so adding a
 * reaction to an event never touches gameplay code.
 *
 *   player / weapon ──push──> EventQueue ──drain──> main → audio
 *                                                  (later: hud, fx)
 *
 * Cleared at the start of each tick. Overflow drops the newest event.
 */
#ifndef EVENT_H
#define EVENT_H

#include "math3d.h"
#include "entity.h" /* ENTITY_INDEX_NONE */

#define EVENT_QUEUE_MAX 64

typedef enum {
    EV_WEAPON_FIRED,  /* pos = muzzle */
    EV_WEAPON_IMPACT, /* pos = hit point, entity = hit entity or none */
    EV_FOOTSTEP,      /* pos = feet */
    EV_PICKUP,        /* pos = collector, amount = value collected */
    EV_DOOR_OPENED,   /* pos = user */
    EV_DOOR_CLOSED,   /* pos = user */
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
