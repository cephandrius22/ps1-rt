/*
 * event.h — Queue of things that happened in the game this frame.
 *
 * Gameplay pushes events; presentation (audio, later HUD/particles)
 * drains them. Producers never call consumers directly, so adding a
 * reaction to an event never touches gameplay code.
 *
 *   player / weapon / ai / game ──push──> EventQueue ──drain──> main → audio
 *                                                             (later: fx)
 *
 * game.c also reads this tick's events back (bullet impacts → damage,
 * exit touched → level end).
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
    EV_WEAPON_PICKUP, /* pos = collector, entity = pickup, amount = 1 if newly owned */
    EV_DOOR_OPENED,   /* pos = door, entity = door */
    EV_DOOR_CLOSED,   /* pos = door, entity = door */
    EV_LOCKED,        /* pos/entity = door or button used without its keycard */
    EV_BUTTON_PRESSED, /* pos/entity = the func_button */
    EV_TRIGGERED,     /* pos/entity = trigger or button that fired its target */
    EV_PLAYER_HURT,   /* pos = player eye, amount = damage taken */
    EV_PLAYER_DIED,   /* pos = player eye, amount = final blow */
    EV_ENEMY_ALERT,   /* pos = enemy feet, entity = enemy; spotted the player */
    EV_ENEMY_ATTACK,  /* pos = enemy feet, entity = enemy; strike wind-up */
    EV_ENEMY_PAIN,    /* pos = enemy feet, entity = enemy, amount = damage */
    EV_ENEMY_DIED,    /* pos = enemy feet, entity = enemy */
    EV_LEVEL_EXIT,    /* pos = player eye, entity = exit pad */
    EV_LIGHT_SHOT,    /* pos = bulb, entity = light index (this tick); a bullet broke it */
    EV_EXPLOSION,     /* pos = blast, entity = the (now spent) projectile, amount = splash radius */
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

/* Empty the queue; push appends (dropped silently when full) */
void events_clear(EventQueue *q);
void events_push(EventQueue *q, GameEvent ev);

#endif
