/*
 * menu.h — Title, pause, options and save/load screens.
 *
 * A pure state machine: input in, an action and a drawable view out.
 * main.c carries out the actions (start, save, load, quit) and hands the
 * view to the overlay, so the menu needs no SDL and does no drawing:
 *
 *   UiCmd ─main─> MenuInput ──menu_update()──> MenuAction ──> main.c
 *                                └──menu_view()──> OverlayMenu ──> overlay
 *
 *   TITLE  ──New Game / Load / Options / Quit
 *   PAUSE  ──Resume / Save / Load / Options / Quit      (Esc while playing)
 *     ├─> OPTIONS  sensitivity, volume, field of view   (left/right change)
 *     ├─> SAVE     5 slots
 *     └─> LOAD     5 slots
 *   Esc goes back one screen; from PAUSE it resumes.
 *
 * shell/ sits beside main.c above the layers: it uses platform/settings
 * for the option values and render/overlay for the view's shape.
 */
#ifndef MENU_H
#define MENU_H

#include "settings.h"
#include "overlay.h"
#include <stdbool.h>

#define MENU_SLOTS 5   /* save slots shown (= SAVE_SLOTS) */

typedef enum {
    MENU_CLOSED,
    MENU_TITLE,
    MENU_PAUSE,
    MENU_OPTIONS,
    MENU_SAVE,
    MENU_LOAD,
} MenuScreen;

typedef enum {
    MENU_NONE,
    MENU_RESUME,            /* close the menu, keep playing */
    MENU_NEW_GAME,
    MENU_SAVE_SLOT,         /* .slot = 1..MENU_SLOTS */
    MENU_LOAD_SLOT,
    MENU_QUIT,
    MENU_SETTINGS_CHANGED,  /* apply menu.settings now */
    MENU_SETTINGS_DONE,     /* left the options: write them to disk */
} MenuEvent;

typedef struct {
    MenuEvent event;
    int slot;
} MenuAction;

/* One frame of menu input; the pointer in framebuffer pixels */
typedef struct {
    bool up, down, left, right, select, back;
    bool click, pointer_moved;
    int pointer_x, pointer_y;
} MenuInput;

typedef struct {
    MenuScreen screen;
    MenuScreen parent;      /* where OPTIONS/SAVE/LOAD go back to */
    int selected;
    Settings settings;      /* the options screen edits these */
    bool can_save;          /* main.c: only while playing */
    char slot_labels[MENU_SLOTS][OVERLAY_MENU_TEXT]; /* main.c fills: "EMPTY", "REACTOR 3:12" */
    const char *game_title; /* heading of the title screen */
} Menu;

/* Closed menu with these settings */
void menu_init(Menu *m, Settings settings, const char *game_title);

/* Show a screen, first row selected (TITLE/PAUSE also become where
 * sub-screens go back to) */
void menu_open(Menu *m, MenuScreen screen);

/* Any screen showing (the game pauses meanwhile) */
bool menu_is_open(const Menu *m);

/* React to one frame of input */
MenuAction menu_update(Menu *m, const MenuInput *in);

/* What to draw; .title is NULL while closed */
OverlayMenu menu_view(const Menu *m);

#endif
