/*
 * cmd.h — Per-tick player command: what a controller *wants* to do.
 *
 * The contract between whatever drives a player (keyboard/mouse, AI,
 * demo playback) and game logic. Game code reads only PlayerCmd, never
 * device state, so any producer can drive any actor.
 *
 *   platform/input  ──┐
 *   ai (later)      ──┼──> PlayerCmd ──> player / weapon
 *   demo (later)    ──┘
 */
#ifndef CMD_H
#define CMD_H

#include <stdint.h>

/* Held buttons are set every tick while down. Impulses are set only on
 * the tick they are triggered (e.g. toggle flashlight). */
typedef enum {
    BTN_JUMP       = 1 << 0,
    BTN_ATTACK     = 1 << 1,
    BTN_USE        = 1 << 2,
    BTN_FLASHLIGHT = 1 << 3, /* impulse */
} CmdButton;

#define CMD_IMPULSES (BTN_FLASHLIGHT)

typedef struct {
    float forward;     /* -1..1, + = forward */
    float strafe;      /* -1..1, + = right */
    float yaw_delta;   /* radians, + = turn left */
    float pitch_delta; /* radians, + = look up */
    uint32_t buttons;  /* CmdButton bitmask */
    int8_t weapon_slot;  /* impulse: 1..N selects that weapon, 0 = none */
    int8_t weapon_cycle; /* impulse: +1 next owned weapon, -1 previous (summed) */
} PlayerCmd;

/* Is the button held (or, for impulses, pressed this tick)? */
static inline int cmd_has(const PlayerCmd *cmd, CmdButton btn) {
    return (cmd->buttons & btn) != 0;
}

/* Fold a newer frame's input into a command not yet consumed by a tick.
 * Axes and held buttons take the latest state; look deltas add up and
 * impulses stick, so nothing is lost when a frame runs zero ticks. */
static inline void cmd_accumulate(PlayerCmd *pending, const PlayerCmd *in) {
    pending->forward = in->forward;
    pending->strafe = in->strafe;
    pending->yaw_delta += in->yaw_delta;
    pending->pitch_delta += in->pitch_delta;

    uint32_t impulses = (pending->buttons | in->buttons) & CMD_IMPULSES;
    pending->buttons = (in->buttons & ~(uint32_t)CMD_IMPULSES) | impulses;

    /* Weapon choice: the latest slot pressed wins; wheel steps add up */
    if (in->weapon_slot != 0) {
        pending->weapon_slot = in->weapon_slot;
    }
    pending->weapon_cycle = (int8_t)(pending->weapon_cycle + in->weapon_cycle);
}

/* After a tick used the command: one-shot parts must not repeat, held
 * state carries into further ticks of the same frame. */
static inline void cmd_consume(PlayerCmd *cmd) {
    cmd->yaw_delta = 0;
    cmd->pitch_delta = 0;
    cmd->buttons &= ~(uint32_t)CMD_IMPULSES;
    cmd->weapon_slot = 0;
    cmd->weapon_cycle = 0;
}

#endif
