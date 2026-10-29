#include "test_framework.h"
#include "menu.h"
#include <string.h>

static Menu m;

static MenuAction press(MenuInput in) {
    return menu_update(&m, &in);
}

static const MenuInput UP = {.up = true}, DOWN = {.down = true}, SELECT = {.select = true},
                       BACK = {.back = true}, LEFT = {.left = true}, RIGHT = {.right = true};

TEST(test_menu_title_navigation) {
    menu_init(&m, settings_default(), "Descent");
    menu_open(&m, MENU_TITLE);
    OverlayMenu v = menu_view(&m);
    ASSERT(strcmp(v.title, "Descent") == 0);
    ASSERT(strcmp(v.items[0], "NEW GAME") == 0);

    press(UP);                                   /* wraps to the last row */
    ASSERT(strcmp(menu_view(&m).items[m.selected], "QUIT GAME") == 0);
    ASSERT_EQ_INT(press(SELECT).event, MENU_QUIT);

    press(DOWN);                                 /* back to NEW GAME */
    ASSERT_EQ_INT(press(BACK).event, MENU_NONE); /* nothing behind the title */
    ASSERT_EQ_INT(press(SELECT).event, MENU_NEW_GAME);
    ASSERT(!menu_is_open(&m));
}

/* Options: sliders move in steps and clamp; leaving reports "done" */
TEST(test_menu_options) {
    menu_init(&m, settings_default(), "x");
    menu_open(&m, MENU_PAUSE);
    press(DOWN); press(DOWN); press(DOWN);       /* OPTIONS */
    press(SELECT);
    ASSERT_EQ_INT(m.screen, MENU_OPTIONS);

    float before = m.settings.sensitivity;
    ASSERT_EQ_INT(press(RIGHT).event, MENU_SETTINGS_CHANGED);
    ASSERT(m.settings.sensitivity > before);
    for (int i = 0; i < 100; i++) {
        press(LEFT);
    }
    ASSERT_NEAR(m.settings.sensitivity, SENSITIVITY_MIN, 1e-5f);

    ASSERT_EQ_INT(press(BACK).event, MENU_SETTINGS_DONE);
    ASSERT_EQ_INT(m.screen, MENU_PAUSE);
    ASSERT_EQ_INT(press(BACK).event, MENU_RESUME);   /* pause: back resumes */
    ASSERT(!menu_is_open(&m));
}

/* Saving only while playing; slots report which one */
TEST(test_menu_save_slots) {
    menu_init(&m, settings_default(), "x");
    m.can_save = false;
    menu_open(&m, MENU_PAUSE);
    press(DOWN);
    press(SELECT);
    ASSERT_EQ_INT(m.screen, MENU_PAUSE);        /* refused */

    m.can_save = true;
    press(SELECT);
    ASSERT_EQ_INT(m.screen, MENU_SAVE);
    press(DOWN); press(DOWN);
    MenuAction a = press(SELECT);
    ASSERT_EQ_INT(a.event, MENU_SAVE_SLOT);
    ASSERT_EQ_INT(a.slot, 3);
    ASSERT(strstr(menu_view(&m).items[0], "EMPTY"));
}

/* Mouse: hovering a row selects it, clicking activates it */
TEST(test_menu_mouse) {
    menu_init(&m, settings_default(), "x");
    menu_open(&m, MENU_TITLE);
    OverlayMenu v = menu_view(&m);
    MenuInput hover = {.pointer_moved = true, .pointer_x = OVERLAY_CENTER_X,
                       .pointer_y = v.top_y + 2 * v.line_h};
    press(hover);
    ASSERT_EQ_INT(m.selected, 2);
    MenuInput click = hover;
    click.click = true;
    press(click);
    ASSERT_EQ_INT(m.screen, MENU_OPTIONS);

    MenuInput nowhere = {.click = true, .pointer_y = 5};
    ASSERT_EQ_INT(press(nowhere).event, MENU_NONE);
}

int main(void) {
    TEST_SUITE("menu");
    RUN_TEST(test_menu_title_navigation);
    RUN_TEST(test_menu_options);
    RUN_TEST(test_menu_save_slots);
    RUN_TEST(test_menu_mouse);
    TEST_REPORT();
}
