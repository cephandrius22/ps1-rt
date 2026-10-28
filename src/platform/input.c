#include "input.h"
#include <SDL.h>

void input_init(void) {
    SDL_SetRelativeMouseMode(SDL_TRUE);
}

/* Map a pair of opposing keys to -1..1, e.g. W/S → forward axis */
static float key_axis(const Uint8 *keys, SDL_Scancode pos, SDL_Scancode neg) {
    return (float)keys[pos] - (float)keys[neg];
}

InputStatus input_poll(PlayerCmd *cmd, UiCmd *ui) {
    *cmd = (PlayerCmd){0};
    *ui = (UiCmd){0};
    InputStatus status = INPUT_CONTINUE;
    int mouse_dx = 0;
    int mouse_dy = 0;

    /* Events: quit, impulses, mouse motion */
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            status = INPUT_QUIT;
            continue;
        }

        /* Wheel up = previous weapon, as in Quake */
        if (e.type == SDL_MOUSEWHEEL && e.wheel.y != 0) {
            cmd->weapon_cycle = (int8_t)(cmd->weapon_cycle + (e.wheel.y > 0 ? -1 : 1));
            continue;
        }

        if (e.type == SDL_MOUSEMOTION) {
            mouse_dx += e.motion.xrel;
            mouse_dy += e.motion.yrel;
            continue;
        }

        if (e.type != SDL_KEYDOWN || e.key.repeat) {
            continue;
        }

        if (e.key.keysym.sym == SDLK_ESCAPE) {
            status = INPUT_QUIT;
        }
        if (e.key.keysym.sym >= SDLK_1 && e.key.keysym.sym <= SDLK_9) {
            cmd->weapon_slot = (int8_t)(e.key.keysym.sym - SDLK_0);
        }
        if (e.key.keysym.sym == SDLK_F3) {
            ui->toggle_debug = true;
        }
        if (e.key.keysym.sym == SDLK_f) {
            cmd->buttons |= BTN_FLASHLIGHT;
        }
    }

    /* Held state: movement axes and buttons */
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    cmd->forward = key_axis(keys, SDL_SCANCODE_W, SDL_SCANCODE_S);
    cmd->strafe  = key_axis(keys, SDL_SCANCODE_D, SDL_SCANCODE_A);

    if (keys[SDL_SCANCODE_SPACE]) {
        cmd->buttons |= BTN_JUMP;
    }
    if (keys[SDL_SCANCODE_E]) {
        cmd->buttons |= BTN_USE;
    }
    if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        cmd->buttons |= BTN_ATTACK;
    }

    /* Mouse right/down → turn right/look down */
    cmd->yaw_delta   = -mouse_dx * MOUSE_SENSITIVITY;
    cmd->pitch_delta = -mouse_dy * MOUSE_SENSITIVITY;

    return status;
}
