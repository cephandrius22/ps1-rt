/*
 * overlay.h — 2D layer drawn over the rendered frame (HUD, death tint).
 *
 * render/ never includes game headers. main.c copies the few values the
 * overlay needs into OverlayState, keeping gameplay free of drawing code.
 * The Overlay object only caches textures (viewmodel sheets by name):
 *
 *   Game ──main.c──> OverlayState ──> overlay_draw(Overlay) ──> Framebuffer
 *
 * Draw order: viewmodel, desaturation (low health), flash tint, mode
 * effect (death tint / level-end dim), message, debug text, HUD. An open
 * menu replaces the message and HUD: the frame dims and the menu draws.
 *
 * HUD layout (320x240):
 *
 *   ┌──────────────────────────────┐
 *   │ 7.1 MS 140 FPS               │  debug lines (F3), top left
 *   │ TRI 252/30000                │
 *   │   YOU NEED THE RED KEYCARD   │  message (5x7 font, font.h)
 *   │      PROCESSING PLANT        │  intermission (level end):
 *   │      KILLS    3/4             │  map title, kills, secrets,
 *   │      SECRETS  1/1             │  time, next map
 *   │ ▬▬▬▬▬                ▮ ▮     │  flashlight battery, keycards held
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
    OVERLAY_LEVEL_END,  /* darkened frame, intermission stats centered */
    OVERLAY_EPISODE_END, /* darkened frame, "episode complete" and its title */
} OverlayMode;

/* Debug text is formatted by the caller; the overlay only draws it,
 * colored by how close each value is to its limit */
typedef enum {
    DEBUG_OK,
    DEBUG_WARN,
    DEBUG_OVER,
} DebugSeverity;

#define OVERLAY_DEBUG_LINES 5
#define OVERLAY_MENU_ITEMS  8
#define OVERLAY_MENU_TEXT   40
#define OVERLAY_CENTER_X    (SCREEN_W / 2)
#define OVERLAY_KEYCARDS    4
#define OVERLAY_DEBUG_CHARS 48

typedef struct {
    char text[OVERLAY_DEBUG_CHARS];  /* A-Z, 0-9, space and . - / : */
    DebugSeverity severity;
} DebugLine;

/* A menu screen, laid out by its owner (shell/menu.c): a title and rows
 * of text at fixed heights, one of them highlighted */
typedef struct {
    const char *title;          /* NULL = no menu */
    char items[OVERLAY_MENU_ITEMS][OVERLAY_MENU_TEXT];
    int count;
    int selected;
    int top_y;                  /* y of the first row, pixels */
    int line_h;                 /* pixels between rows */
} OverlayMenu;

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
    float desaturate;     /* 0..1: drain the frame's color (low health) */
    float flash;          /* 0..1: blend the frame toward flash_color (hits) */
    Vec3 flash_color;
    float weapon_kick;    /* current recoil, radians (viewmodel, later) */
    const char *message;  /* notice centered near the top; NULL = none */
    const char *title;    /* episode title (episode end screen) */

    /* Intermission (level end) */
    const char *map_title;  /* the map just finished */
    const char *next_title; /* the map coming up; NULL = none */
    int secrets;
    int secrets_total;
    int time_seconds;       /* spent on the map */
    bool show_battery;    /* draw the flashlight battery bar */
    float battery;        /* its charge, 0..1 */
    int keycard_count;    /* keycard icons, drawn in this order */
    Vec3 keycard_colors[OVERLAY_KEYCARDS];
    OverlayMenu menu;     /* drawn last, over a dimmed frame, without the HUD */
    int debug_count;      /* 0 = debug overlay hidden */
    DebugLine debug[OVERLAY_DEBUG_LINES];
} OverlayState;

/* Texture cache for viewmodels; generated on first use */
typedef struct {
    Texture views[PTEX_COUNT];
    bool generated[PTEX_COUNT];
} Overlay;

/* Empty cache; overlay_free releases the generated textures */
void overlay_init(Overlay *o);
void overlay_free(Overlay *o);

/* Draw over a finished frame (order in the header comment) */
void overlay_draw(Overlay *o, Framebuffer *fb, const OverlayState *state);

#endif
