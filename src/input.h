/*
 * input.h — SDL2 keyboard and mouse input handling.
 *
 * Captures WASD keys for movement, mouse relative motion for look,
 * and Escape/window close for quitting. Uses SDL relative mouse mode
 * to capture the cursor.
 */
#ifndef INPUT_H
#define INPUT_H

#include <SDL.h>
#include <stdbool.h>

typedef struct {
    bool forward, back, left, right;
    bool jump;
    bool shoot;
    bool use;
    bool flashlight_toggle; /* true on the frame F is pressed */
    int mouse_dx, mouse_dy;
} InputState;

void input_init(void);
void input_update(InputState *state, bool *running);

#endif
