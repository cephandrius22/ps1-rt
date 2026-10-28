#include "test_framework.h"
#include "projectile.h"

#define TICK (1.0f / 60.0f)

static Scene s;
static Mesh world;
static EventQueue ev;

/* Floor at y=0 over [-20, 20]; optional wall across x in [-5, 5] at z=wall_z,
 * 4 m tall, facing +Z */
static void build_world(bool with_wall, float wall_z) {
    scene_init(&s);
    mesh_init(&world);
    Vec3 a = vec3(-20, 0, -20), b = vec3(20, 0, -20), c = vec3(20, 0, 20), d = vec3(-20, 0, 20);
    mesh_add_tri(&world, (Triangle){.v0 = a, .v1 = d, .v2 = c, .normal = vec3(0, 1, 0)});
    mesh_add_tri(&world, (Triangle){.v0 = a, .v1 = c, .v2 = b, .normal = vec3(0, 1, 0)});
    if (with_wall) {
        Vec3 w0 = vec3(-5, 0, wall_z), w1 = vec3(5, 0, wall_z);
        Vec3 w2 = vec3(5, 4, wall_z), w3 = vec3(-5, 4, wall_z);
        mesh_add_tri(&world, (Triangle){.v0 = w0, .v1 = w1, .v2 = w2, .normal = vec3(0, 0, 1)});
        mesh_add_tri(&world, (Triangle){.v0 = w0, .v1 = w2, .v2 = w3, .normal = vec3(0, 0, 1)});
    }
    scene_build_static(&s, world.tris, world.count);
    events_clear(&ev);
}

static void free_world(void) {
    mesh_free(&world);
    scene_free(&s);
}

/* Player standing at (x, 0, z) */
static Player player_at(float x, float z) {
    Player p = player_create(vec3(x, PLAYER_HEIGHT + PLAYER_EYE_OFFSET, z));
    p.foot_y = 0;
    p.on_ground = true;
    return p;
}

static void run(Player *p, int ticks) {
    for (int i = 0; i < ticks; i++) {
        scene_rebuild_dynamic(&s);
        projectile_update(&s, p, TICK, &ev);
    }
}

static int count_events(GameEventType type) {
    int n = 0;
    for (int i = 0; i < ev.count; i++) {
        n += ev.items[i].type == type;
    }
    return n;
}

static int add_ghoul(float x, float z, float health) {
    char args[32];
    snprintf(args, sizeof(args), "health=%g", health);
    return entity_add(&s.entities, entity_spawn(ENTITY_GHOUL, vec3(x, 0, z), args));
}

TEST(test_projectile_flies_at_speed) {
    build_world(false, 0);
    Player p = player_at(0, 10);
    int i = projectile_fire(&s, PROJECTILE_ROCKET, vec3(0, 1, 0), vec3(0, 0, -1),
                            PROJECTILE_OWNER_PLAYER);
    run(&p, 1);
    ASSERT_NEAR(s.entities.items[i].position.z, -projectile_def(PROJECTILE_ROCKET)->speed * TICK, 1e-4f);
    ASSERT(s.entities.items[i].active);
    free_world();
}

TEST(test_projectile_explodes_on_wall) {
    build_world(true, -3);
    Player p = player_at(0, 20);
    int lights = s.lights.count;
    int i = projectile_fire(&s, PROJECTILE_ROCKET, vec3(0, 1, 0), vec3(0, 0, -1),
                            PROJECTILE_OWNER_PLAYER);
    run(&p, 30);
    ASSERT(!s.entities.items[i].active);
    ASSERT_EQ_INT(count_events(EV_EXPLOSION), 1);
    ASSERT(s.lights.count > lights); /* explosion light still burning */
    free_world();
}

/* Direct hit = direct damage + splash at point blank */
TEST(test_projectile_direct_hit) {
    build_world(false, 0);
    Player p = player_at(0, 15);
    int g = add_ghoul(0, -4, 500);
    projectile_fire(&s, PROJECTILE_ROCKET, vec3(0, 1, 0), vec3(0, 0, -1), PROJECTILE_OWNER_PLAYER);
    run(&p, 30);
    float lost = 500 - s.entities.items[g].health;
    ASSERT(lost > projectile_def(PROJECTILE_ROCKET)->damage);
    ASSERT_EQ_INT(count_events(EV_ENEMY_PAIN), 2); /* direct, then splash */
    free_world();
}

/* Splash reaches a ghoul beside the blast, not one behind the wall */
TEST(test_projectile_splash_needs_clear_line) {
    build_world(true, -3);
    Player p = player_at(0, 15);
    int beside = add_ghoul(1.2f, -2.5f, 500);
    int behind = add_ghoul(0.0f, -4.0f, 500);
    projectile_fire(&s, PROJECTILE_ROCKET, vec3(0, 1, 0), vec3(0, 0, -1), PROJECTILE_OWNER_PLAYER);
    run(&p, 30);
    ASSERT(s.entities.items[beside].health < 500);
    ASSERT_NEAR(s.entities.items[behind].health, 500.0f, 1e-6f);
    free_world();
}

/* Rockets into a wall at your feet hurt you */
TEST(test_projectile_self_damage) {
    build_world(true, -1);
    Player p = player_at(0, 0);
    projectile_fire(&s, PROJECTILE_ROCKET, vec3(0, 1, 0), vec3(0, 0, -1), PROJECTILE_OWNER_PLAYER);
    run(&p, 10);
    ASSERT(p.health < PLAYER_MAX_HEALTH);
    free_world();
}

/* Spit launched from inside its shooter's sprite flies out and hits the player */
TEST(test_projectile_spit_hits_player_not_owner) {
    build_world(false, 0);
    Player p = player_at(0, 5);
    int shooter = add_ghoul(0, 0, 100);
    scene_rebuild_dynamic(&s);
    projectile_fire(&s, PROJECTILE_SPIT, vec3(0, 1.2f, 0), vec3(0, 0, 1), shooter);
    run(&p, 60);
    ASSERT_NEAR(s.entities.items[shooter].health, 100.0f, 1e-6f);
    ASSERT_NEAR(p.health, PLAYER_MAX_HEALTH - projectile_def(PROJECTILE_SPIT)->damage, 1e-4f);
    free_world();
}

TEST(test_projectile_reuses_slots) {
    build_world(true, -2);
    Player p = player_at(0, 20);
    int first = projectile_fire(&s, PROJECTILE_ROCKET, vec3(0, 1, 0), vec3(0, 0, -1),
                                PROJECTILE_OWNER_PLAYER);
    run(&p, 30);
    int second = projectile_fire(&s, PROJECTILE_ROCKET, vec3(0, 1, 0), vec3(0, 0, -1),
                                 PROJECTILE_OWNER_PLAYER);
    ASSERT_EQ_INT(first, second);
    ASSERT_EQ_INT(s.entities.count, 1);
    free_world();
}

TEST(test_projectile_bursts_when_life_runs_out) {
    build_world(false, 0);
    Player p = player_at(0, 50);
    int i = projectile_fire(&s, PROJECTILE_SPIT, vec3(0, 3, 0), vec3(1, 0, 0), PROJECTILE_OWNER_PLAYER);
    run(&p, (int)(projectile_def(PROJECTILE_SPIT)->life / TICK) + 2);
    ASSERT(!s.entities.items[i].active);
    ASSERT_EQ_INT(count_events(EV_EXPLOSION), 1);
    free_world();
}

int main(void) {
    TEST_SUITE("projectile");
    RUN_TEST(test_projectile_flies_at_speed);
    RUN_TEST(test_projectile_explodes_on_wall);
    RUN_TEST(test_projectile_direct_hit);
    RUN_TEST(test_projectile_splash_needs_clear_line);
    RUN_TEST(test_projectile_self_damage);
    RUN_TEST(test_projectile_spit_hits_player_not_owner);
    RUN_TEST(test_projectile_reuses_slots);
    RUN_TEST(test_projectile_bursts_when_life_runs_out);
    TEST_REPORT();
}
