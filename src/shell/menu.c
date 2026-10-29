#include "menu.h"
#include <stdio.h>

#define MENU_TOP_Y   92   /* first row, framebuffer pixels */
#define MENU_LINE_H  20   /* row spacing (rows are 14 px tall at scale 2) */

#define SENSITIVITY_STEP 0.1f
#define VOLUME_STEP      0.1f
#define FOV_STEP         5.0f
#define PERCENT          100.0f

/* --- Rows per screen --- */

typedef enum {
    ROW_RESUME, ROW_NEW_GAME, ROW_SAVE, ROW_LOAD, ROW_OPTIONS, ROW_QUIT,
    ROW_SENSITIVITY, ROW_VOLUME, ROW_FOV, ROW_SLOT, ROW_BACK,
} RowKind;

typedef struct {
    RowKind kind;
    int slot;   /* ROW_SLOT: 1..MENU_SLOTS */
} Row;

/* The rows of the current screen, top to bottom; returns how many */
static int screen_rows(const Menu *m, Row rows[OVERLAY_MENU_ITEMS]) {
    int n = 0;
    switch (m->screen) {
    case MENU_TITLE:
        rows[n++] = (Row){.kind = ROW_NEW_GAME};
        rows[n++] = (Row){.kind = ROW_LOAD};
        rows[n++] = (Row){.kind = ROW_OPTIONS};
        rows[n++] = (Row){.kind = ROW_QUIT};
        break;
    case MENU_PAUSE:
        rows[n++] = (Row){.kind = ROW_RESUME};
        rows[n++] = (Row){.kind = ROW_SAVE};
        rows[n++] = (Row){.kind = ROW_LOAD};
        rows[n++] = (Row){.kind = ROW_OPTIONS};
        rows[n++] = (Row){.kind = ROW_QUIT};
        break;
    case MENU_OPTIONS:
        rows[n++] = (Row){.kind = ROW_SENSITIVITY};
        rows[n++] = (Row){.kind = ROW_VOLUME};
        rows[n++] = (Row){.kind = ROW_FOV};
        rows[n++] = (Row){.kind = ROW_BACK};
        break;
    case MENU_SAVE:
    case MENU_LOAD:
        for (int s = 1; s <= MENU_SLOTS; s++) {
            rows[n++] = (Row){.kind = ROW_SLOT, .slot = s};
        }
        rows[n++] = (Row){.kind = ROW_BACK};
        break;
    case MENU_CLOSED:
        break;
    }
    return n;
}

static const char *screen_title(const Menu *m) {
    switch (m->screen) {
    case MENU_TITLE:   return m->game_title;
    case MENU_PAUSE:   return "PAUSED";
    case MENU_OPTIONS: return "OPTIONS";
    case MENU_SAVE:    return "SAVE GAME";
    case MENU_LOAD:    return "LOAD GAME";
    case MENU_CLOSED:  return NULL;
    }
    return NULL;
}

static void row_text(const Menu *m, Row row, char *out, int size) {
    const Settings *s = &m->settings;
    switch (row.kind) {
    case ROW_RESUME:      snprintf(out, (size_t)size, "RESUME"); break;
    case ROW_NEW_GAME:    snprintf(out, (size_t)size, "NEW GAME"); break;
    case ROW_SAVE:        snprintf(out, (size_t)size, m->can_save ? "SAVE GAME" : "SAVE GAME (NOT NOW)"); break;
    case ROW_LOAD:        snprintf(out, (size_t)size, "LOAD GAME"); break;
    case ROW_OPTIONS:     snprintf(out, (size_t)size, "OPTIONS"); break;
    case ROW_QUIT:        snprintf(out, (size_t)size, "QUIT GAME"); break;
    case ROW_SENSITIVITY: snprintf(out, (size_t)size, "< MOUSE SPEED %.1f >", s->sensitivity); break;
    case ROW_VOLUME:      snprintf(out, (size_t)size, "< VOLUME %d%% >", (int)(s->volume * PERCENT + 0.5f)); break;
    case ROW_FOV:         snprintf(out, (size_t)size, "< FIELD OF VIEW %d >", (int)(s->fov + 0.5f)); break;
    case ROW_SLOT:        snprintf(out, (size_t)size, "%d  %s", row.slot, m->slot_labels[row.slot - 1]); break;
    case ROW_BACK:        snprintf(out, (size_t)size, "BACK"); break;
    }
}

/* --- Navigation --- */

void menu_init(Menu *m, Settings settings, const char *game_title) {
    *m = (Menu){.screen = MENU_CLOSED, .settings = settings, .game_title = game_title};
    for (int i = 0; i < MENU_SLOTS; i++) {
        snprintf(m->slot_labels[i], OVERLAY_MENU_TEXT, "EMPTY");
    }
}

void menu_open(Menu *m, MenuScreen screen) {
    if (screen == MENU_TITLE || screen == MENU_PAUSE) {
        m->parent = screen;
    }
    m->screen = screen;
    m->selected = 0;
}

bool menu_is_open(const Menu *m) {
    return m->screen != MENU_CLOSED;
}

/* Back one screen: sub-screens to their parent, pause resumes, the
 * title stays (there's nothing behind it) */
static MenuAction go_back(Menu *m) {
    MenuScreen from = m->screen;
    if (from == MENU_PAUSE) {
        m->screen = MENU_CLOSED;
        return (MenuAction){MENU_RESUME, 0};
    }
    if (from == MENU_TITLE) {
        return (MenuAction){MENU_NONE, 0};
    }
    menu_open(m, m->parent);
    return (MenuAction){from == MENU_OPTIONS ? MENU_SETTINGS_DONE : MENU_NONE, 0};
}

/* Nudge a slider row by dir (-1 / +1), clamped to its range */
static MenuAction adjust(Menu *m, RowKind kind, int dir) {
    Settings *s = &m->settings;
    if (kind == ROW_SENSITIVITY) {
        s->sensitivity += dir * SENSITIVITY_STEP;
    } else if (kind == ROW_VOLUME) {
        s->volume += dir * VOLUME_STEP;
    } else if (kind == ROW_FOV) {
        s->fov += dir * FOV_STEP;
    } else {
        return (MenuAction){MENU_NONE, 0};
    }
    settings_clamp(s);
    return (MenuAction){MENU_SETTINGS_CHANGED, 0};
}

static MenuAction activate(Menu *m, Row row) {
    switch (row.kind) {
    case ROW_RESUME:
        m->screen = MENU_CLOSED;
        return (MenuAction){MENU_RESUME, 0};
    case ROW_NEW_GAME:
        m->screen = MENU_CLOSED;
        return (MenuAction){MENU_NEW_GAME, 0};
    case ROW_SAVE:
        if (m->can_save) {
            menu_open(m, MENU_SAVE);
        }
        return (MenuAction){MENU_NONE, 0};
    case ROW_LOAD:
        menu_open(m, MENU_LOAD);
        return (MenuAction){MENU_NONE, 0};
    case ROW_OPTIONS:
        menu_open(m, MENU_OPTIONS);
        return (MenuAction){MENU_NONE, 0};
    case ROW_QUIT:
        return (MenuAction){MENU_QUIT, 0};
    case ROW_SLOT:
        return (MenuAction){m->screen == MENU_SAVE ? MENU_SAVE_SLOT : MENU_LOAD_SLOT, row.slot};
    case ROW_BACK:
        return go_back(m);
    case ROW_SENSITIVITY:
    case ROW_VOLUME:
    case ROW_FOV:
        return adjust(m, row.kind, 1);
    }
    return (MenuAction){MENU_NONE, 0};
}

/* Row under the pointer, or -1 */
static int row_at(int count, int y) {
    int i = (y - MENU_TOP_Y + MENU_LINE_H / 2) / MENU_LINE_H;
    return y >= MENU_TOP_Y - MENU_LINE_H / 2 && i >= 0 && i < count ? i : -1;
}

MenuAction menu_update(Menu *m, const MenuInput *in) {
    Row rows[OVERLAY_MENU_ITEMS];
    int count = screen_rows(m, rows);
    if (count == 0) {
        return (MenuAction){MENU_NONE, 0};
    }

    if (in->back) {
        return go_back(m);
    }

    /* Mouse: hover selects; a click activates, or nudges a slider
     * toward the side of the screen it lands on */
    int hover = row_at(count, in->pointer_y);
    if ((in->pointer_moved || in->click) && hover >= 0) {
        m->selected = hover;
    }
    if (in->click && hover >= 0) {
        bool slider = rows[hover].kind == ROW_SENSITIVITY || rows[hover].kind == ROW_VOLUME ||
                      rows[hover].kind == ROW_FOV;
        if (slider) {
            return adjust(m, rows[hover].kind, in->pointer_x < OVERLAY_CENTER_X ? -1 : 1);
        }
        return activate(m, rows[hover]);
    }

    if (in->up) {
        m->selected = (m->selected + count - 1) % count;
    }
    if (in->down) {
        m->selected = (m->selected + 1) % count;
    }
    if (in->left || in->right) {
        return adjust(m, rows[m->selected].kind, in->right ? 1 : -1);
    }
    if (in->select) {
        return activate(m, rows[m->selected]);
    }
    return (MenuAction){MENU_NONE, 0};
}

OverlayMenu menu_view(const Menu *m) {
    OverlayMenu view = {.title = screen_title(m), .selected = m->selected,
                        .top_y = MENU_TOP_Y, .line_h = MENU_LINE_H};
    Row rows[OVERLAY_MENU_ITEMS];
    view.count = screen_rows(m, rows);
    for (int i = 0; i < view.count; i++) {
        row_text(m, rows[i], view.items[i], OVERLAY_MENU_TEXT);
    }
    return view;
}
