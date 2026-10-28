/*
 * projectile.h — Rockets and spit: slow, visible shots that explode.
 *
 * Projectiles are ENTITY_PROJECTILE entities (world draws them as ghost
 * sprites); this module flies them each tick. Behaviour per kind lives in
 * a ProjectileDef table, like weapons and enemies:
 *
 *   projectile_fire() ──> scene_spawn() (reuses a spent slot)
 *   projectile_update(), per tick, per projectile:
 *     trace this tick's step: walls + solid sprites (TRACE_SOLID), skipping
 *     the shooter; and the player's body (not in the scene)
 *       miss ──> move, glow light, age (burst when life runs out)
 *       hit  ──> explode: direct damage to what was hit,
 *                splash to everything in radius with a clear line,
 *                explosion light, EV_EXPLOSION
 *
 * Splash hurts the shooter too (Quake rules: mind your rockets).
 */
#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "scene.h"
#include "player.h"
#include "event.h"

typedef enum {
    PROJECTILE_NONE,    /* hitscan weapons use this */
    PROJECTILE_ROCKET,
    PROJECTILE_SPIT,
    PROJECTILE_KIND_COUNT
} ProjectileKind;

typedef struct {
    float speed;            /* m/s */
    float damage;           /* direct hit */
    float splash_radius;    /* meters; 0 = no splash */
    float splash_damage;    /* at the center, falling linearly to 0 at the radius */
    float hit_radius;       /* how fat it is when hitting the player */
    float life;             /* seconds before it bursts on its own */
    int frame;              /* ProjectileFrame in the projectiles sheet */
    float size;             /* sprite size, meters */

    Vec3 glow_color;        /* light carried in flight; intensity 0 = none */
    float glow_intensity;
    float glow_radius;

    Vec3 burst_color;       /* explosion light */
    float burst_intensity;
    float burst_radius;
    float burst_time;
} ProjectileDef;

/* Table row for a kind; PROJECTILE_NONE / out of range fall back to the rocket */
const ProjectileDef *projectile_def(ProjectileKind kind);

/* Launch from `from` along dir (normalized here). owner = shooter's entity
 * index or PROJECTILE_OWNER_PLAYER. Returns the entity index, or -1. */
int projectile_fire(Scene *s, ProjectileKind kind, Vec3 from, Vec3 dir, int owner);

/* Fly every live projectile one tick */
void projectile_update(Scene *s, Player *p, float dt, EventQueue *ev);

#endif
