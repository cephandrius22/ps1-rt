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

typedef struct {
    float forward;     /* -1..1, + = forward */
    float strafe;      /* -1..1, + = right */
    float yaw_delta;   /* radians, + = turn left */
    float pitch_delta; /* radians, + = look up */
    uint32_t buttons;  /* CmdButton bitmask */
} PlayerCmd;

static inline int cmd_has(const PlayerCmd *cmd, CmdButton btn) {
    return (cmd->buttons & btn) != 0;
}

#endif
