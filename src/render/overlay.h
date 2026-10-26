/*
 * overlay.h — 2D layer drawn over the rendered frame (viewmodel, HUD).
 *
 * render/ never includes game headers. main.c copies the few values the
 * overlay needs into OverlayState, keeping gameplay free of drawing code:
 *
 *   Game ──main.c──> OverlayState ──> overlay_draw() ──> Framebuffer
 *
 * Drawing is empty for now; Phase 1 adds the viewmodel and HUD here.
 */
#ifndef OVERLAY_H
#define OVERLAY_H

#include "render.h"
#include <stdbool.h>

typedef struct {
    float weapon_kick;    /* current recoil, radians */
    bool weapon_fired;    /* a shot left the barrel on the last tick */
} OverlayState;

void overlay_draw(Framebuffer *fb, const OverlayState *state);

#endif
