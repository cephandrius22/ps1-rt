/*
 * damage.h — The one path for "an entity got hurt".
 *
 * Bullets (resolve_impacts in game.c) and explosions (projectile.c) both
 * call this, so pain/death events — and everything keyed off them (kill
 * count, gibs, sounds) — can't diverge between weapons:
 *
 *   bullet impact ──┐
 *                   ├──> damage_entity() ──> entity_damage() hook
 *   explosion ──────┘                    └──> EV_ENEMY_PAIN / EV_ENEMY_DIED
 *
 * game.c counts kills from EV_ENEMY_DIED events.
 */
#ifndef DAMAGE_H
#define DAMAGE_H

#include "scene.h"
#include "event.h"

/* Damage entity index by amount; pushes the matching pain/died event */
DamageResult damage_entity(Scene *s, int index, float amount, EventQueue *ev);

#endif
