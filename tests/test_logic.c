#include "test_framework.h"
#include <string.h>
#include "logic.h"
#include "ai.h"

#define TICK (1.0f / 60.0f)

static Scene s;
static Mesh floor_mesh;
static EventQueue ev;

/*  Floor [-10, 10]², nav grid built; one door brush across z = -3:
 *
 *    z -3   ══door══  (x -1..1, 2.5 m tall, rises)
 *    z  0      P      player at the origin, facing -Z
 */
static int build_world(const char *door_args) {
    scene_init(&s);
    mesh_init(&floor_mesh);
    Vec3 a = vec3(-10, 0, -10), b = vec3(10, 0, -10);
    Vec3 c = vec3(10, 0, 10),   d = vec3(-10, 0, 10);
    mesh_add_tri(&floor_mesh, (Triangle){.v0 = a, .v1 = d, .v2 = c, .normal = vec3(0, 1, 0)});
    mesh_add_tri(&floor_mesh, (Triangle){.v0 = a, .v1 = c, .v2 = b, .normal = vec3(0, 1, 0)});
    scene_build_static(&s, floor_mesh.tris, floor_mesh.count);
    nav_build(&s.nav, &s.static_bvh);

    Entity door = entity_spawn_brush(ENTITY_DOOR, vec3(0, 1.25f, -3), vec3(1, 1.25f, 0.1f),
                                     door_args);
    events_clear(&ev);
    return entity_add(&s.entities, door);
}

static void free_world(void) {
    mesh_free(&floor_mesh);
    scene_free(&s);
}

static Player player_at(Vec3 feet) {
    Player p = player_create(vec3(feet.x, feet.y + PLAYER_HEIGHT + PLAYER_EYE_OFFSET, feet.z));
    p.foot_y = feet.y;
    p.on_ground = true;
    return p;
}

static void run(const Player *p, float seconds) {
    for (float t = 0; t < seconds; t += TICK) {
        logic_update(&s, p, TICK, &ev);
    }
}

static const Entity *door(int i) {
    return &s.entities.items[i];
}

static bool has_event(GameEventType type) {
    for (int i = 0; i < ev.count; i++) {
        if (ev.items[i].type == type) {
            return true;
        }
    }
    return false;
}

TEST(test_logic_use_opens_then_closes) {
    int d = build_world("wait=1");
    Player p = player_at(vec3(0, 0, -1));

    logic_use(&s, &p, &ev);
    ASSERT_EQ_INT(door(d)->mover.state, MOVER_OPENING);
    ASSERT(has_event(EV_DOOR_OPENED));

    run(&p, 2.0f);  /* 2.375 m at 2.5 m/s: open within 1 s, then waits 1 s */
    ASSERT(door(d)->mover.state == MOVER_CLOSING || door(d)->mover.state == MOVER_OPEN);
    run(&p, 2.0f);
    ASSERT_EQ_INT(door(d)->mover.state, MOVER_CLOSED);
    ASSERT_NEAR(door(d)->mover.offset, 0.0f, 1e-6f);
    ASSERT(has_event(EV_DOOR_CLOSED));
    free_world();
}

TEST(test_logic_locked_doors_ignore_use) {
    int keyed = build_world("key=red");
    Player p = player_at(vec3(0, 0, -1));
    logic_use(&s, &p, &ev);
    ASSERT_EQ_INT(door(keyed)->mover.state, MOVER_CLOSED);
    free_world();

    int named = build_world("targetname=gate");
    logic_use(&s, &p, &ev);
    ASSERT_EQ_INT(door(named)->mover.state, MOVER_CLOSED);
    free_world();
}

TEST(test_logic_keycard_opens_keyed_door) {
    int d = build_world("key=red");
    Player p = player_at(vec3(0, 0, -1));

    logic_use(&s, &p, &ev);
    ASSERT_EQ_INT(door(d)->mover.state, MOVER_CLOSED);
    ASSERT(has_event(EV_LOCKED));

    inventory_give_keycard(&p.inv, KEYCARD_RED);
    logic_use(&s, &p, &ev);
    ASSERT_EQ_INT(door(d)->mover.state, MOVER_OPENING);
    free_world();
}

/* trigger_once over the player's path: fires the named door, shows its
 * message once, then is spent */
TEST(test_logic_trigger_once_opens_named_door) {
    int d = build_world("targetname=gate");
    Entity trig = entity_spawn_brush(ENTITY_TRIGGER_ONCE, vec3(0, 1, 2), vec3(2, 1, 0.5f),
                                     "target=gate message=\"The gate grinds open\"");
    int t = entity_add(&s.entities, trig);

    Player outside = player_at(vec3(0, 0, 5));
    run(&outside, TICK);
    ASSERT_EQ_INT(door(d)->mover.state, MOVER_CLOSED);

    Player inside = player_at(vec3(0, 0, 2));
    run(&inside, TICK);
    ASSERT_EQ_INT(door(d)->mover.state, MOVER_OPENING);
    ASSERT(has_event(EV_TRIGGERED));
    ASSERT(!s.entities.items[t].active);
    ASSERT(strcmp(s.entities.items[t].message, "The gate grinds open") == 0);
    free_world();
}

/* trigger_multiple fires again only after its wait */
TEST(test_logic_trigger_multiple_rearms) {
    build_world("");
    int t = entity_add(&s.entities, entity_spawn_brush(ENTITY_TRIGGER_MULTIPLE, vec3(0, 1, 2),
                                                       vec3(2, 1, 0.5f), "wait=0.5"));
    Player inside = player_at(vec3(0, 0, 2));
    int fired = 0;
    for (int i = 0; i < 60; i++) {   /* one second inside */
        events_clear(&ev);
        logic_update(&s, &inside, TICK, &ev);
        fired += has_event(EV_TRIGGERED);
    }
    ASSERT_EQ_INT(fired, 2);
    ASSERT(s.entities.items[t].active);
    free_world();
}

/* Button: E presses it, its target fires (hidden monster appears, a
 * light toggles), then it pops back */
TEST(test_logic_button_fires_target) {
    build_world("");
    s.entities.items[0].active = false; /* door out of the way */
    Entity button = entity_spawn_brush(ENTITY_BUTTON, vec3(0, 1.2f, -1.5f),
                                       vec3(0.2f, 0.2f, 0.1f), "target=ambush wait=0.5");
    int b = entity_add(&s.entities, button);
    int ghoul = entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(5, 0, -5),
                                                     "targetname=ambush"));
    light_add(&s.lights, light_from_keys(vec3(0, 2, 0), vec3(1, 1, 1), 1, 5, "targetname=ambush"));
    ASSERT(!s.entities.items[ghoul].active); /* named: waits hidden */

    Player p = player_at(vec3(0, 0, 0));
    logic_use(&s, &p, &ev);
    ASSERT(has_event(EV_BUTTON_PRESSED));
    ASSERT(s.entities.items[ghoul].active);
    ASSERT_EQ_INT(s.entities.items[ghoul].enemy.state, ENEMY_CHASE);
    ASSERT(s.lights.items[0].off);

    run(&p, 2.0f);
    ASSERT_EQ_INT(s.entities.items[b].mover.state, MOVER_CLOSED);
    free_world();
}

/* Doom: a closing door that meets a body goes back up */
TEST(test_logic_door_reverses_on_player) {
    int d = build_world("wait=0");
    Player away = player_at(vec3(0, 0, 5));
    s.entities.items[d].mover.state = MOVER_OPENING;
    run(&away, 1.5f);  /* opened, and already closing */
    ASSERT_EQ_INT(door(d)->mover.state, MOVER_CLOSING);

    Player under = player_at(vec3(0, 0, -3));
    run(&under, TICK);
    ASSERT_EQ_INT(door(d)->mover.state, MOVER_OPENING);
    free_world();
}

TEST(test_logic_monsters_open_plain_doors) {
    int d = build_world("");
    Player p = player_at(vec3(0, 0, 5));
    int ghoul = entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(0, 0, -3.6f), ""));

    run(&p, TICK);  /* idle: just standing there */
    ASSERT_EQ_INT(door(d)->mover.state, MOVER_CLOSED);

    enemy_set_state(&s.entities.items[ghoul], ENEMY_CHASE);
    run(&p, TICK);
    ASSERT_EQ_INT(door(d)->mover.state, MOVER_OPENING);
    free_world();

    /* Keyed doors stop them */
    d = build_world("key=blue");
    ghoul = entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(0, 0, -3.6f), ""));
    enemy_set_state(&s.entities.items[ghoul], ENEMY_CHASE);
    run(&p, TICK);
    ASSERT_EQ_INT(door(d)->mover.state, MOVER_CLOSED);
    free_world();
}

/* A closed door stops sound; open, it carries */
TEST(test_logic_closed_door_muffles) {
    int d = build_world("wait=-1");
    Player p = player_at(vec3(0, 0, 0));
    int ghoul = entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(0, 0, -4.5f), ""));

    /* Wall line across z = -3 with only the doorway open: block the rest
     * of the hall as locked doors so sound must pass the door */
    nav_set_door(&s.nav, vec3(-10, 0, -3.1f), vec3(-1.3f, 2.5f, -2.9f), NAV_DOOR_LOCKED, 1);
    nav_set_door(&s.nav, vec3(1.3f, 0, -3.1f), vec3(10, 2.5f, -2.9f), NAV_DOOR_LOCKED, 1);

    run(&p, TICK);
    ai_hear(&s, vec3(0, 0, 0), 20.0f, &ev);
    ASSERT_EQ_INT(s.entities.items[ghoul].enemy.state, ENEMY_IDLE);

    s.entities.items[d].mover.state = MOVER_OPENING;
    run(&p, TICK);
    ai_hear(&s, vec3(0, 0, 0), 20.0f, &ev);
    ASSERT_EQ_INT(s.entities.items[ghoul].enemy.state, ENEMY_CHASE);
    free_world();
}

int main(void) {
    TEST_SUITE("logic");
    RUN_TEST(test_logic_use_opens_then_closes);
    RUN_TEST(test_logic_locked_doors_ignore_use);
    RUN_TEST(test_logic_keycard_opens_keyed_door);
    RUN_TEST(test_logic_door_reverses_on_player);
    RUN_TEST(test_logic_trigger_once_opens_named_door);
    RUN_TEST(test_logic_trigger_multiple_rearms);
    RUN_TEST(test_logic_button_fires_target);
    RUN_TEST(test_logic_monsters_open_plain_doors);
    RUN_TEST(test_logic_closed_door_muffles);
    TEST_REPORT();
}
