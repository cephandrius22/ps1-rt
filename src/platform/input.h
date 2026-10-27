/*
 * input.h — Keyboard/mouse → PlayerCmd.
 *
 * Owns key bindings and mouse sensitivity. Uses SDL relative mouse mode
 * to capture the cursor. SDL stays inside this module.
 */
#ifndef INPUT_H
#define INPUT_H

#include "cmd.h"

#define MOUSE_SENSITIVITY 0.003f /* radians per mouse count */

typedef enum {
    INPUT_CONTINUE,
    INPUT_QUIT,
} InputStatus;

/* Capture the mouse (relative mode) */
void input_init(void);

/* Drain pending events and build this frame's command; game_update()
 * spreads it over however many ticks the frame runs. */
InputStatus input_poll(PlayerCmd *cmd);

#endif
