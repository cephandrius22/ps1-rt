#include "display.h"
#include <SDL.h>
#include <stdio.h>

bool display_open(Display *d, const char *title, int width, int height, int scale) {
    *d = (Display){.width = width, .height = height};
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL video: %s\n", SDL_GetError());
        return false;
    }

    d->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 width * scale, height * scale, SDL_WINDOW_SHOWN);
    if (d->window) {
        d->renderer = SDL_CreateRenderer(d->window, -1,
                                         SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    }
    if (d->renderer) {
        d->texture = SDL_CreateTexture(d->renderer, SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STREAMING, width, height);
    }
    if (!d->texture) {
        fprintf(stderr, "display: %s\n", SDL_GetError());
        display_close(d);
        return false;
    }
    return true;
}

void display_present(Display *d, const uint32_t *pixels) {
    SDL_UpdateTexture(d->texture, NULL, pixels, d->width * (int)sizeof(uint32_t));
    SDL_RenderClear(d->renderer);
    SDL_RenderCopy(d->renderer, d->texture, NULL, NULL);
    SDL_RenderPresent(d->renderer);
}

void display_close(Display *d) {
    if (d->texture) {
        SDL_DestroyTexture(d->texture);
    }
    if (d->renderer) {
        SDL_DestroyRenderer(d->renderer);
    }
    if (d->window) {
        SDL_DestroyWindow(d->window);
    }
    *d = (Display){0};
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
