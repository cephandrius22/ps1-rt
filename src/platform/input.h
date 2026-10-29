/*
 * input.h — Keyboard/mouse → PlayerCmd (play) and UiCmd (menus, F-keys).
 *
 * Owns key bindings and mouse sensitivity. While playing, the mouse is
 * captured (relative mode) and turns the view; in menus it's released,
 * and UiCmd carries its position and clicks. Esc is "back": it opens
 * the pause menu or leaves the current one. Closing the window quits.
 * SDL stays inside this module.
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

    /* Menus: one-shot presses this frame */
    bool back;          /* Esc */
    bool up, down;      /* arrows, W/S */
    bool left, right;   /* arrows, A/D */
    bool select;        /* Enter, Space */
    bool click;         /* left button (menus only: the mouse is free) */
    bool pointer_moved; /* the free mouse moved this frame */
    int pointer_x, pointer_y; /* free mouse, window pixels */
} UiCmd;

/* Capture the mouse (relative mode) */
void input_init(void);

/* Captured (play: hidden, turns the view) or free (menus: a pointer) */
typedef enum {
    MOUSE_CAPTURED,
    MOUSE_FREE,
} MouseMode;

/* Switch between play (captured) and menus (free pointer) */
void input_set_mouse(MouseMode mode);

/* Mouse speed multiplier (settings: 1 = MOUSE_SENSITIVITY) */
void input_set_sensitivity(float scale);

/* Drain pending events and build this frame's command; game_update()
 * spreads it over however many ticks the frame runs. */
InputStatus input_poll(PlayerCmd *cmd, UiCmd *ui);

#endif
