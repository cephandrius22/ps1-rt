/*
 * overlay.h — 2D layer drawn over the rendered frame (HUD, death tint).
 *
 * render/ never includes game headers. main.c copies the few values the
 * overlay needs into OverlayState, keeping gameplay free of drawing code:
 *
 *   Game ──main.c──> OverlayState ──> overlay_draw() ──> Framebuffer
 *
 * HUD layout (320x240):
 *
 *   ┌──────────────────────────────┐
 *   │                              │
 *   │            3/4               │  kills/total (level end only)
 *   │                              │
 *   │ + 100                    50  │  health (left), ammo (right)
 *   └──────────────────────────────┘
 */
#ifndef OVERLAY_H
#define OVERLAY_H

#include "render.h"

typedef enum {
    OVERLAY_PLAYING,
    OVERLAY_DEAD,       /* red-tinted frame */
    OVERLAY_LEVEL_END,  /* darkened frame, kill count centered */
} OverlayMode;

typedef struct {
    OverlayMode mode;
    int health;
    int ammo;
    int kills;
    int kills_total;
    float weapon_kick;    /* current recoil, radians (viewmodel, later) */
} OverlayState;

/* Draw over a finished frame: mode effect (tint/dim) first, then HUD */
void overlay_draw(Framebuffer *fb, const OverlayState *state);

#endif
