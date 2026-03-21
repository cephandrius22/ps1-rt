#include "test_framework.h"
#include "../src/threadpool.h"
#include "../src/render.h"
#include "../src/scene.h"
#include <string.h>

/* --- Thread pool lifecycle tests --- */

TEST(test_threadpool_create_destroy) {
    ThreadPool pool;
    int ret = threadpool_create(&pool, 4);
    ASSERT(ret == 0);
    ASSERT(pool.worker_count == 4);
    threadpool_destroy(&pool);
}

TEST(test_threadpool_invalid_count) {
    ThreadPool pool;
    ASSERT(threadpool_create(&pool, 0) != 0);
    ASSERT(threadpool_create(&pool, THREADPOOL_MAX_WORKERS + 1) != 0);
}

TEST(test_threadpool_cpu_count) {
    int n = threadpool_cpu_count();
    ASSERT(n >= 1);
    ASSERT(n <= THREADPOOL_MAX_WORKERS);
}

/* --- Dispatch tests --- */

typedef struct {
    int id;
    int *results;
} WorkItem;

static void set_id(void *arg) {
    WorkItem *item = arg;
    item->results[item->id] = item->id * item->id;
}

TEST(test_threadpool_dispatch) {
    ThreadPool pool;
    threadpool_create(&pool, 4);

    int results[4] = {0};
    WorkItem items[4];
    void *args[4];
    for (int i = 0; i < 4; i++) {
        items[i].id = i;
        items[i].results = results;
        args[i] = &items[i];
    }

    threadpool_dispatch(&pool, set_id, args, 4);

    ASSERT(results[0] == 0);
    ASSERT(results[1] == 1);
    ASSERT(results[2] == 4);
    ASSERT(results[3] == 9);

    threadpool_destroy(&pool);
}

TEST(test_threadpool_dispatch_multiple_rounds) {
    ThreadPool pool;
    threadpool_create(&pool, 2);

    int results[2] = {0};
    WorkItem items[2];
    void *args[2];

    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 2; i++) {
            items[i].id = i;
            items[i].results = results;
            args[i] = &items[i];
        }
        threadpool_dispatch(&pool, set_id, args, 2);
        ASSERT(results[0] == 0);
        ASSERT(results[1] == 1);
    }

    threadpool_destroy(&pool);
}

/* --- MT render correctness test --- */

static void build_test_floor(Mesh *m) {
    mesh_init(m);
    Triangle t = {
        .v0 = vec3(-10, 0, -10), .v1 = vec3(10, 0, -10), .v2 = vec3(0, 0, 10),
        .uv0 = {0,0}, .uv1 = {1,0}, .uv2 = {0.5f,1},
        .normal = vec3(0, 1, 0), .material_id = 0
    };
    mesh_add_tri(m, t);
}

TEST(test_mt_render_matches_st) {
    /* Build a simple scene */
    Scene scene;
    scene_init(&scene);

    Mesh m;
    build_test_floor(&m);
    scene_build_static(&scene, m.tris, m.count);

    /* Add a light */
    light_add(&scene.lights, (PointLight){
        .position = vec3(0, 3, 0), .color = vec3(1, 1, 1),
        .intensity = 2.0f, .radius = 15.0f, .cast_shadows = false,
        .flicker_speed = 0, .flicker_amount = 0
    });

    Camera cam = camera_create(vec3(0, 2, 0), 0, -0.5f, 1.2f);
    float time = 1.0f;

    /* Render single-threaded */
    Framebuffer fb_st;
    render_scene_lit(&fb_st, &cam, &scene, time, false);

    /* Render multi-threaded */
    ThreadPool pool;
    threadpool_create(&pool, 4);
    Framebuffer fb_mt;
    render_scene_lit_mt(&fb_mt, &cam, &scene, time, &pool, false);
    threadpool_destroy(&pool);

    /* Compare pixel-by-pixel */
    int mismatches = 0;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        if (fb_st.pixels[i] != fb_mt.pixels[i])
            mismatches++;
    }
    ASSERT(mismatches == 0);

    mesh_free(&m);
    scene_free(&scene);
}

TEST(test_mt_render_various_thread_counts) {
    Scene scene;
    scene_init(&scene);
    Mesh m;
    build_test_floor(&m);
    scene_build_static(&scene, m.tris, m.count);

    Camera cam = camera_create(vec3(0, 2, 0), 0, 0, 1.2f);

    /* Get reference from single-threaded */
    Framebuffer fb_ref;
    render_scene_lit(&fb_ref, &cam, &scene, 0, false);

    /* Test with 1, 2, 3, 7 threads (including non-power-of-two) */
    int counts[] = {1, 2, 3, 7};
    for (int c = 0; c < 4; c++) {
        ThreadPool pool;
        threadpool_create(&pool, counts[c]);
        Framebuffer fb;
        render_scene_lit_mt(&fb, &cam, &scene, 0, &pool, false);
        threadpool_destroy(&pool);

        for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
            ASSERT(fb.pixels[i] == fb_ref.pixels[i]);
    }

    mesh_free(&m);
    scene_free(&scene);
}

int main(void) {
    TEST_SUITE("threadpool");
    RUN_TEST(test_threadpool_create_destroy);
    RUN_TEST(test_threadpool_invalid_count);
    RUN_TEST(test_threadpool_cpu_count);
    RUN_TEST(test_threadpool_dispatch);
    RUN_TEST(test_threadpool_dispatch_multiple_rounds);
    RUN_TEST(test_mt_render_matches_st);
    RUN_TEST(test_mt_render_various_thread_counts);
    TEST_REPORT();
}
