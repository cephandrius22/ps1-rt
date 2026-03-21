#include "input.h"

void input_init(void) {
    SDL_SetRelativeMouseMode(SDL_TRUE);
}

void input_update(InputState *state, bool *running) {
    state->mouse_dx = 0;
    state->mouse_dy = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT)
            *running = false;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
            *running = false;
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
}
