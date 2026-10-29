#include "test_framework.h"
#include "ai.h"
#include <math.h>

#define TICK (1.0f / 60.0f)

static Scene s;
static Mesh floor_mesh;
static EventQueue ev;

/* Flat floor at y=0 spanning [-20, 20] on X/Z; optional wall box */
static void build_world(bool with_wall) {
    scene_init(&s);
    mesh_init(&floor_mesh);
    Vec3 a = vec3(-20, 0, -20), b = vec3(20, 0, -20);
    Vec3 c = vec3(20, 0, 20),   d = vec3(-20, 0, 20);
    mesh_add_tri(&floor_mesh, (Triangle){.v0 = a, .v1 = d, .v2 = c, .normal = vec3(0, 1, 0)});
    mesh_add_tri(&floor_mesh, (Triangle){.v0 = a, .v1 = c, .v2 = b, .normal = vec3(0, 1, 0)});

    /* Wall: a tall quad across x in [-5, 5] at z = -4, facing +Z */
    if (with_wall) {
        Vec3 w0 = vec3(-5, 0, -4), w1 = vec3(5, 0, -4);
        Vec3 w2 = vec3(5, 4, -4),  w3 = vec3(-5, 4, -4);
        mesh_add_tri(&floor_mesh, (Triangle){.v0 = w0, .v1 = w1, .v2 = w2, .normal = vec3(0, 0, 1)});
        mesh_add_tri(&floor_mesh, (Triangle){.v0 = w0, .v1 = w2, .v2 = w3, .normal = vec3(0, 0, 1)});
    }
    scene_build_static(&s, floor_mesh.tris, floor_mesh.count);
}

static void free_world(void) {
    mesh_free(&floor_mesh);
    scene_free(&s);
}

static Player player_at_origin(void) {
    Player p = player_create(vec3(0, PLAYER_HEIGHT + PLAYER_EYE_OFFSET, 0));
    p.on_ground = true;
    p.foot_y = 0;
    return p;
}

/* Rebuild sprites then run the AI, like one game tick */
static void run_ai(Player *p, int ticks) {
    for (int i = 0; i < ticks; i++) {
        scene_rebuild_dynamic(&s);
        ai_update(&s, p, TICK, &ev);
    }
}

static bool has_event(const EventQueue *q, GameEventType type) {
    for (int i = 0; i < q->count; i++) {
        if (q->items[i].type == type) {
            return true;
        }
    }
    return false;
}

TEST(test_ai_def_table) {
    ASSERT(enemy_def(ENTITY_GHOUL) != NULL);
    ASSERT(enemy_def(ENTITY_PROP) == NULL);
}

TEST(test_ai_spots_and_chases) {
    build_world(false);
    entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(0, 0, -8), ""));
    Player p = player_at_origin();
    events_clear(&ev);

    run_ai(&p, 1);
    ASSERT(s.entities.items[0].enemy.state == ENEMY_CHASE);
    ASSERT(has_event(&ev, EV_ENEMY_ALERT));

    run_ai(&p, 60);
    ASSERT(s.entities.items[0].position.z > -8.0f + 1.0f); /* closed in */
    free_world();
}

TEST(test_ai_wall_blocks_sight) {
    build_world(true);
    entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(0, 0, -8), ""));
    Player p = player_at_origin();

    run_ai(&p, 30);
    ASSERT(s.entities.items[0].enemy.state == ENEMY_IDLE);
    free_world();
}

TEST(test_ai_attacks_in_reach) {
    build_world(false);
    entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(0, 0, -1.0f), ""));
    Player p = player_at_origin();
    const EnemyDef *def = enemy_def(ENTITY_GHOUL);
    events_clear(&ev);

    run_ai(&p, 2);
    ASSERT(s.entities.items[0].enemy.state == ENEMY_ATTACK);
    ASSERT(has_event(&ev, EV_ENEMY_ATTACK));

    /* Strike lands after the windup */
    run_ai(&p, (int)(def->windup / TICK) + 2);
    ASSERT_NEAR(p.health, PLAYER_MAX_HEALTH - def->damage, 1e-4f);
    free_world();
}

TEST(test_ai_pain_then_death) {
    build_world(false);
    entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(0, 0, -8), "health=50"));
    Player p = player_at_origin();

    ASSERT(entity_damage(&s.entities, 0, 20) == DAMAGE_HURT);
    ASSERT(s.entities.items[0].enemy.state == ENEMY_PAIN);
    run_ai(&p, 30);
    ASSERT(s.entities.items[0].enemy.state == ENEMY_CHASE);

    ASSERT(entity_damage(&s.entities, 0, 40) == DAMAGE_KILLED);
    ASSERT(s.entities.items[0].enemy.state == ENEMY_DEAD);
    ASSERT(entity_damage(&s.entities, 0, 40) == DAMAGE_IGNORED);

    /* Corpses stay put and no longer block bullets */
    Vec3 corpse = s.entities.items[0].position;
    run_ai(&p, 60);
    ASSERT_NEAR(s.entities.items[0].position.z, corpse.z, 1e-6f);
    ASSERT(s.sprite_count == 1);
    ASSERT(s.sprites[0].solidity == SPRITE_GHOST);
    free_world();
}

static int count_projectiles(void) {
    int n = 0;
    for (int i = 0; i < s.entities.count; i++) {
        n += s.entities.items[i].active && s.entities.items[i].type == ENTITY_PROJECTILE;
    }
    return n;
}

/* In range with a clear line: stands still and spits once per cooldown */
TEST(test_ai_spitter_holds_and_spits) {
    build_world(false);
    entity_add(&s.entities, entity_spawn(ENTITY_SPITTER, vec3(0, 0, -6), ""));
    Player p = player_at_origin();
    const EnemyDef *def = enemy_def(ENTITY_SPITTER);

    run_ai(&p, (int)((def->windup + 0.1f) / TICK));
    ASSERT_NEAR(s.entities.items[0].position.z, -6.0f, 1e-6f); /* held position */
    ASSERT_EQ_INT(count_projectiles(), 1);

    /* Not again until the cooldown (+ next windup) has passed */
    run_ai(&p, (int)((def->cooldown * 0.5f) / TICK));
    ASSERT_EQ_INT(count_projectiles(), 1);
    run_ai(&p, (int)((def->cooldown + def->windup) / TICK));
    ASSERT_EQ_INT(count_projectiles(), 2);
    free_world();
}

/* Too far: walks in until within its preferred range */
TEST(test_ai_spitter_approaches_from_afar) {
    build_world(false);
    entity_add(&s.entities, entity_spawn(ENTITY_SPITTER, vec3(0, 0, -16), ""));
    Player p = player_at_origin();
    float range = enemy_def(ENTITY_SPITTER)->preferred_range;

    run_ai(&p, 60 * 6);
    float dist = -s.entities.items[0].position.z;
    ASSERT(dist < 16.0f);
    ASSERT(dist >= range - 0.5f); /* stopped near its range, didn't rush in */
    free_world();
}

/* Behind a wall: never spits, even once alerted */
TEST(test_ai_spitter_needs_line_of_sight) {
    build_world(true);
    entity_add(&s.entities, entity_spawn(ENTITY_SPITTER, vec3(0, 0, -6), ""));
    enemy_set_state(&s.entities.items[0], ENEMY_CHASE);
    Player p = player_at_origin();

    run_ai(&p, 60 * 3);
    ASSERT_EQ_INT(count_projectiles(), 0);
    free_world();
}

TEST(test_ai_paths_around_wall) {
    build_world(true);
    nav_build(&s.nav, &s.static_bvh);
    entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(0, 0, -8), ""));
    enemy_set_state(&s.entities.items[0], ENEMY_CHASE);
    Player p = player_at_origin();

    /* Straight at the player is into the wall; the path goes round it */
    run_ai(&p, 600);
    Vec3 at = s.entities.items[0].position;
    ASSERT(sqrtf(at.x * at.x + at.z * at.z) < 1.5f);
    free_world();
}

TEST(test_ai_hears_around_walls) {
    build_world(true);
    nav_build(&s.nav, &s.static_bvh);
    entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(0, 0, -8), ""));
    Vec3 shot = vec3(0, 0, 0);
    events_clear(&ev);

    /* 8 m as the crow flies, ~13 m walking round the wall: muffled */
    ai_hear(&s, shot, 10.0f, &ev);
    ASSERT(s.entities.items[0].enemy.state == ENEMY_IDLE);

    ai_hear(&s, shot, 25.0f, &ev);
    ASSERT(s.entities.items[0].enemy.state == ENEMY_CHASE);
    ASSERT(has_event(&ev, EV_ENEMY_ALERT));
    free_world();
}

/* Two ghouls spawned on top of each other spread out while chasing */
TEST(test_ai_monsters_spread_out) {
    build_world(false);
    entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(0, 0, -12), ""));
    entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(0.05f, 0, -12), ""));
    enemy_set_state(&s.entities.items[0], ENEMY_CHASE);
    enemy_set_state(&s.entities.items[1], ENEMY_CHASE);
    Player p = player_at_origin();

    run_ai(&p, 60);
    Vec3 a = s.entities.items[0].position, b = s.entities.items[1].position;
    float apart = sqrtf((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
    ASSERT(apart > 0.6f);
    free_world();
}

/* Enemies fall onto the floor and chase across it */
TEST(test_ai_enemy_falls_to_floor) {
    build_world(false);
    entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(0, 3, -15), ""));
    Player p = player_at_origin();
    run_ai(&p, 120);
    ASSERT_NEAR(s.entities.items[0].position.y, 0.0f, 1e-4f);
    ASSERT_NEAR(s.entities.items[0].enemy.fall_speed, 0.0f, 1e-6f);
    free_world();
}

int main(void) {
    TEST_SUITE("ai");
    RUN_TEST(test_ai_def_table);
    RUN_TEST(test_ai_spots_and_chases);
    RUN_TEST(test_ai_wall_blocks_sight);
    RUN_TEST(test_ai_attacks_in_reach);
    RUN_TEST(test_ai_pain_then_death);
    RUN_TEST(test_ai_paths_around_wall);
    RUN_TEST(test_ai_hears_around_walls);
    RUN_TEST(test_ai_monsters_spread_out);
    RUN_TEST(test_ai_enemy_falls_to_floor);
    RUN_TEST(test_ai_spitter_holds_and_spits);
    RUN_TEST(test_ai_spitter_approaches_from_afar);
    RUN_TEST(test_ai_spitter_needs_line_of_sight);
    TEST_REPORT();
}
