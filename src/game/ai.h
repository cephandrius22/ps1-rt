/*
 * ai.h — Enemy behavior: sight, chase, melee attack.
 *
 * World code stores each enemy's EnemyState and draws it; this module
 * decides transitions, once per game tick:
 *
 *   IDLE ──sees player──> CHASE ──in reach──> ATTACK ──windup done──> CHASE
 *                           ^                   (strike if still in reach)
 *        any hit ──> PAIN ──┘ after pain_time
 *        health 0 ──> DEAD (corpse, stays)
 *
 * Behavior stats live in an EnemyDef table keyed by EntityType; health
 * is set at spawn by the world (entity_types.c).
 */
#ifndef AI_H
#define AI_H

#include "scene.h"
#include "player.h"
#include "event.h"

typedef struct {
    float speed;        /* units per second while chasing */
    float sight_range;  /* max distance to notice the player */
    float melee_range;  /* start an attack within this distance */
    float reach;        /* strike lands if still within this distance */
    float damage;
    float windup;       /* seconds from attack start to strike */
    float cooldown;     /* seconds after a strike before the next */
    float pain_time;    /* seconds stunned after a hit */
    float radius;       /* collision radius vs walls */
} EnemyDef;

/* Behavior stats for an entity type, or NULL if it has no AI */
const EnemyDef *enemy_def(EntityType type);

/* Advance every enemy one tick toward/against the player */
void ai_update(Scene *scene, Player *player, float dt, EventQueue *ev);

#endif
