#include "input.h"

void input_init(void) {
    SDL_SetRelativeMouseMode(SDL_TRUE);
}

void input_update(InputState *state, bool *running) {
    state->mouse_dx = 0;
    state->mouse_dy = 0;
    state->flashlight_toggle = false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT)
            *running = false;
        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE)
                *running = false;
            if (e.key.keysym.sym == SDLK_f && !e.key.repeat)
                state->flashlight_toggle = true;
        }
        if (e.type == SDL_MOUSEMOTION) {
            state->mouse_dx += e.motion.xrel;
            state->mouse_dy += e.motion.yrel;
        }
    }

    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    state->forward = keys[SDL_SCANCODE_W];
    state->back    = keys[SDL_SCANCODE_S];
    state->left    = keys[SDL_SCANCODE_A];
    state->right   = keys[SDL_SCANCODE_D];
    state->jump    = keys[SDL_SCANCODE_SPACE];
    state->shoot   = SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT);
    state->use     = keys[SDL_SCANCODE_E];
}
