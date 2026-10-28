/*
 * overlay.h — 2D layer drawn over the rendered frame (HUD, death tint).
 *
 * render/ never includes game headers. main.c copies the few values the
 * overlay needs into OverlayState, keeping gameplay free of drawing code.
 * The Overlay object only caches textures (viewmodel sheets by name):
 *
 *   Game ──main.c──> OverlayState ──> overlay_draw(Overlay) ──> Framebuffer
 *
 * Draw order: viewmodel, mode effect (death tint / level-end dim), debug
 * text, HUD numbers.
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
#include "proctex.h"

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

/* The held weapon, positioned and lit by main.c */
typedef struct {
    const char *texture;  /* viewmodel sheet name (WeaponDef); NULL = none */
    int frame;            /* ViewFrame: idle or firing */
    float offset_x;       /* pixels: bob sway */
    float offset_y;       /* pixels down: bob, recoil, lowering for a switch */
    float light;          /* brightness multiplier, ~0.25 (dark) .. 1.5 */
} ViewmodelState;

typedef struct {
    OverlayMode mode;
    ViewmodelState viewmodel;
    int health;
    int ammo;
    int kills;
    int kills_total;
    float weapon_kick;    /* current recoil, radians (viewmodel, later) */
    int debug_count;      /* 0 = debug overlay hidden */
    DebugLine debug[OVERLAY_DEBUG_LINES];
} OverlayState;

/* Draw over a finished frame: mode effect (tint/dim) first, then HUD */
/* Texture cache for viewmodels; generated on first use */
typedef struct {
    Texture views[PTEX_COUNT];
    bool generated[PTEX_COUNT];
} Overlay;

void overlay_init(Overlay *o);
void overlay_free(Overlay *o);

/* Draw over a finished frame (order in the header comment) */
void overlay_draw(Overlay *o, Framebuffer *fb, const OverlayState *state);

#endif
