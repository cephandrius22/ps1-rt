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
 * game.c counts kills from EV_ENEMY_DIED events. Hits also shove
 * monsters away from where the damage came from, harder the bigger the
 * hit (a shotgun blast up close, a rocket), corpses included.
 */
#ifndef DAMAGE_H
#define DAMAGE_H

#include "scene.h"
#include "event.h"

/* Damage entity index by amount, coming from `from` (shooter's eye,
 * blast center); pushes the matching pain/died event */
DamageResult damage_entity(Scene *s, int index, float amount, Vec3 from, EventQueue *ev);

#endif
