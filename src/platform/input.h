/*
 * input.h — Keyboard/mouse → PlayerCmd.
 *
 * Owns key bindings and mouse sensitivity. Uses SDL relative mouse mode
 * to capture the cursor. SDL stays inside this module.
 */
#ifndef INPUT_H
#define INPUT_H

#include "cmd.h"
#include <stdbool.h>

#define MOUSE_SENSITIVITY 0.003f /* radians per mouse count, at scale 1 */

typedef enum {
    INPUT_CONTINUE,
    INPUT_QUIT,
} InputStatus;

/* Presentation-only requests (never reach game logic, unlike PlayerCmd) */
typedef struct {
    bool toggle_debug;  /* F3 pressed this frame */
    bool toggle_affine; /* F4: PS1 texture warp on/off */
    bool quick_save;    /* F5 */
    bool quick_load;    /* F9 */
} UiCmd;

/* Capture the mouse (relative mode) */
void input_init(void);

/* Mouse speed multiplier (settings: 1 = MOUSE_SENSITIVITY) */
void input_set_sensitivity(float scale);

/* Drain pending events and build this frame's command; game_update()
 * spreads it over however many ticks the frame runs. */
InputStatus input_poll(PlayerCmd *cmd, UiCmd *ui);

#endif
