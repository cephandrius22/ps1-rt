/*
 * player.h — First-person player controller with physics and collision.
 *
 * Applies a PlayerCmd: yaw-relative movement and look (pitch clamped).
 * Includes gravity, ground snapping, wall collision with sliding, and
 * step climbing. Owns health and ammo; pickups apply their TouchEffect here.
 */
#ifndef PLAYER_H
#define PLAYER_H

#include "math3d.h"
#include "cmd.h"
#include "event.h"
#include "inventory.h"

struct Scene;

#define PLAYER_HEIGHT     1.6f
#define PLAYER_EYE_OFFSET 0.1f
#define PLAYER_RADIUS     0.3f
#define PLAYER_SPEED      5.0f
#define PLAYER_GRAVITY    18.0f
#define PLAYER_JUMP_VEL   7.0f
#define STEP_HEIGHT       0.6f
#define PLAYER_PITCH_MAX  1.4f
#define PLAYER_STEP_DIST  2.5f  /* distance walked per footstep event */
#define PLAYER_MAX_HEALTH 100.0f

typedef struct {
    Pose view;            /* eye position + look direction */
    Vec3 velocity;
    float foot_y;
    bool on_ground;
    bool flashlight_on;
    float walk_distance;  /* distance since last footstep event */
    float move_speed;     /* horizontal speed last tick, m/s (viewmodel bob) */
    float health;         /* 0 = dead */
    Inventory inv;        /* ammo pools */
} Player;

/* New player at full health with starting ammo. pos is the eye, not
 * the feet: feet = pos.y - PLAYER_HEIGHT - PLAYER_EYE_OFFSET. */
Player player_create(Vec3 pos);
/* One tick: look, move with collision, footsteps, touch and use entities */
void player_update(Player *p, const PlayerCmd *cmd, float dt, struct Scene *scene,
                   EventQueue *ev);

/* Apply damage; pushes EV_PLAYER_HURT, or EV_PLAYER_DIED when health
 * reaches 0. Ignored once dead. */
void player_damage(Player *p, float amount, EventQueue *ev);

static inline bool player_alive(const Player *p) {
    return p->health > 0;
}

#endif
