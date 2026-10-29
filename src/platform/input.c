#include "input.h"
#include <SDL.h>

void input_init(void) {
    input_set_mouse(MOUSE_CAPTURED);
}

void input_set_mouse(MouseMode mode) {
    SDL_SetRelativeMouseMode(mode == MOUSE_CAPTURED ? SDL_TRUE : SDL_FALSE);
}

/* Menu keys: arrows (and WASD), Enter/Space, Esc */
static void menu_key(SDL_Keycode key, UiCmd *ui) {
    ui->back |= key == SDLK_ESCAPE;
    ui->up |= key == SDLK_UP || key == SDLK_w;
    ui->down |= key == SDLK_DOWN || key == SDLK_s;
    ui->left |= key == SDLK_LEFT || key == SDLK_a;
    ui->right |= key == SDLK_RIGHT || key == SDLK_d;
    ui->select |= key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE;
}

/* Map a pair of opposing keys to -1..1, e.g. W/S → forward axis */
static float key_axis(const Uint8 *keys, SDL_Scancode pos, SDL_Scancode neg) {
    return (float)keys[pos] - (float)keys[neg];
}

static float sensitivity_scale = 1.0f;

void input_set_sensitivity(float scale) {
    sensitivity_scale = scale;
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
            ui->pointer_moved = true;
            ui->pointer_x = e.motion.x;
            ui->pointer_y = e.motion.y;
            continue;
        }

        /* A click only means something to menus (the mouse is free then) */
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT &&
            !SDL_GetRelativeMouseMode()) {
            ui->click = true;
            ui->pointer_x = e.button.x;
            ui->pointer_y = e.button.y;
            continue;
        }

        if (e.type != SDL_KEYDOWN || e.key.repeat) {
            continue;
        }

        menu_key(e.key.keysym.sym, ui);
        if (e.key.keysym.sym >= SDLK_1 && e.key.keysym.sym <= SDLK_9) {
            cmd->weapon_slot = (int8_t)(e.key.keysym.sym - SDLK_0);
        }
        if (e.key.keysym.sym == SDLK_F3) {
            ui->toggle_debug = true;
        }
        if (e.key.keysym.sym == SDLK_F4) {
            ui->toggle_affine = true;
        }
        if (e.key.keysym.sym == SDLK_F5) {
            ui->quick_save = true;
        }
        if (e.key.keysym.sym == SDLK_F9) {
            ui->quick_load = true;
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
    cmd->yaw_delta   = -mouse_dx * MOUSE_SENSITIVITY * sensitivity_scale;
    cmd->pitch_delta = -mouse_dy * MOUSE_SENSITIVITY * sensitivity_scale;

    return status;
}
