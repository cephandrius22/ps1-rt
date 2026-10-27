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
 *   │ 7.1 MS 140 FPS               │  debug lines (F3), top left
 *   │ TRI 252/30000                │
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

/* Debug text is formatted by the caller; the overlay only draws it,
 * colored by how close each value is to its limit */
typedef enum {
    DEBUG_OK,
    DEBUG_WARN,
    DEBUG_OVER,
} DebugSeverity;

#define OVERLAY_DEBUG_LINES 5
#define OVERLAY_DEBUG_CHARS 48

typedef struct {
    char text[OVERLAY_DEBUG_CHARS];  /* A-Z, 0-9, space and . - / : */
    DebugSeverity severity;
} DebugLine;

typedef struct {
    OverlayMode mode;
    int health;
    int ammo;
    int kills;
    int kills_total;
    float weapon_kick;    /* current recoil, radians (viewmodel, later) */
    int debug_count;      /* 0 = debug overlay hidden */
    DebugLine debug[OVERLAY_DEBUG_LINES];
} OverlayState;

/* Draw over a finished frame: mode effect (tint/dim) first, then HUD */
void overlay_draw(Framebuffer *fb, const OverlayState *state);

#endif
