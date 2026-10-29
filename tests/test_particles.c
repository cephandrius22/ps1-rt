#include "test_framework.h"
#include "particles.h"

static ParticleSystem ps;
static Framebuffer fb; /* large: keep off the stack */

#define BG 0xFF101010u

static void clear_frame(float depth) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        fb.pixels[i] = BG;
        fb.depth[i] = depth;
    }
}

static int changed_pixels(void) {
    int n = 0;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        n += fb.pixels[i] != BG;
    }
    return n;
}

TEST(test_particles_burst_and_cap) {
    particles_init(&ps, 1);
    particles_burst(&ps, PARTICLE_BLOOD, vec3(0, 1, 0), vec3(0, 1, 0), 10, 2, 0.5f, 0);
    ASSERT_EQ_INT(ps.count, 10);

    particles_burst(&ps, PARTICLE_GIB, vec3(0, 1, 0), vec3(0, 1, 0), PARTICLES_MAX, 2, 0.5f, 0);
    ASSERT_EQ_INT(ps.count, PARTICLES_MAX); /* overflow dropped */
}

TEST(test_particles_fall_land_and_expire) {
    particles_init(&ps, 2);
    particles_burst(&ps, PARTICLE_GIB, vec3(0, 2, 0), vec3(1, 0, 0), 5, 1, 0, 0.5f);

    particles_update(&ps, 0.1f);
    ASSERT(ps.items[0].vel.y < 0); /* gravity */

    for (int i = 0; i < 30; i++) {
        particles_update(&ps, 0.05f);
    }
    for (int i = 0; i < ps.count; i++) {
        ASSERT_NEAR(ps.items[i].pos.y, 0.5f, 1e-6f); /* landed on its floor */
    }

    for (int i = 0; i < 100; i++) {
        particles_update(&ps, 0.05f);
    }
    ASSERT_EQ_INT(ps.count, 0); /* all expired */
}

TEST(test_particles_sparks_float) {
    particles_init(&ps, 3);
    particles_burst(&ps, PARTICLE_SPARK, vec3(0, 1, 0), vec3(1, 0, 0), 1, 1, 0, 0);
    particles_update(&ps, 0.05f);
    ASSERT_NEAR(ps.items[0].vel.y, 0.0f, 1e-6f); /* no gravity */
}

/* A particle 3 m ahead draws over a far wall but not behind a near one */
TEST(test_particles_depth_test) {
    Environment env = environment_default();
    Camera cam = camera_create(vec3(0, 0, 0), 0, 0, 1.2f);
    particles_init(&ps, 4);
    particles_burst(&ps, PARTICLE_GIB, vec3(0, 0, -3), vec3(0, 1, 0), 1, 0, 0, -10);

    clear_frame(10.0f);
    particles_draw(&ps, &fb, &cam, &env);
    ASSERT(changed_pixels() > 0);

    clear_frame(1.0f);
    particles_draw(&ps, &fb, &cam, &env);
    ASSERT_EQ_INT(changed_pixels(), 0);
}

int main(void) {
    TEST_SUITE("particles");
    RUN_TEST(test_particles_burst_and_cap);
    RUN_TEST(test_particles_fall_land_and_expire);
    RUN_TEST(test_particles_sparks_float);
    RUN_TEST(test_particles_depth_test);
    TEST_REPORT();
}
