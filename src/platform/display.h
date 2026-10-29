/*
 * display.h — The game window: shows a finished frame, scaled up.
 *
 * The renderer draws into a small ARGB framebuffer; the display uploads
 * it to a streaming texture and lets the GPU scale it to the window
 * (nearest neighbour, so pixels stay chunky):
 *
 *   Framebuffer (320x240 ARGB) ──display_present()──> window (x scale)
 *
 * SDL stays inside platform/; callers see only this.
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

typedef struct {
    struct SDL_Window *window;
    struct SDL_Renderer *renderer;
    struct SDL_Texture *texture;
    int width, height;          /* framebuffer size in pixels */
} Display;

/* Open a window `scale` times the framebuffer size, vsynced. False (with
 * a message on stderr) on failure; nothing to close then. */
bool display_open(Display *d, const char *title, int width, int height, int scale);

/* Show width x height ARGB pixels (0xAARRGGBB), waiting for vsync */
void display_present(Display *d, const uint32_t *pixels);

void display_close(Display *d);

#endif
