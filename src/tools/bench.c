#include "bench.h"
#include "game.h"
#include "render.h"
#include "camera.h"
#include "threadpool.h"
#include "clock.h"
#include "math3d.h"
#include <stdlib.h>
#include <string.h>

#define BENCH_MAPS_MAX   32
#define BENCH_VIEWS_MAX  16
#define BENCH_SPAWN_DIRS 4      /* spawn looking north, east, south, west */
#define BENCH_REPS       9      /* renders per view (odd: a true median) */
#define BENCH_SOLO_REPS  2      /* single-thread renders per view */
#define BENCH_TICKS      300    /* 5 s of game time */
#define BENCH_EYE        1.6f   /* enemy viewpoints, meters above the feet */
#define BENCH_FOV        (70.0f * (float)MATH_PI / 180.0f)
#define BENCH_TIME       1.0f   /* render time (flicker phase): fixed */
#define MS               1000.0

static const char *const SHIPPED_MAPS[] = {
    "levels/e1m1.map", "levels/e1m2.map", "levels/e1m3.map",
    "levels/arena.map", "levels/chapel.map",
};

typedef struct {
    const char *map;
    int tris, lights, sprites, views;
    double frame_ms, worst_ms, solo_ms, tick_ms;
} BenchResult;

static Framebuffer fb; /* 600 KB: keep off the stack */

/* The fixed viewpoints of a loaded map; returns how many */
static int bench_views(const Game *g, Pose views[BENCH_VIEWS_MAX]) {
    int n = 0;
    for (int k = 0; k < BENCH_SPAWN_DIRS; k++) {
        views[n] = g->player.view;
        views[n].yaw += k * (float)MATH_PI / 2;
        views[n].pitch = 0;
        n++;
    }

    const EntityList *list = &g->scene.entities;
    for (int i = 0; i < list->count && n < BENCH_VIEWS_MAX; i++) {
        const Entity *e = &list->items[i];
        if (!e->active || !entity_def(e->type)->damage) {
            continue;
        }
        views[n++] = (Pose){.pos = vec3_add(e->position, vec3(0, BENCH_EYE, 0)),
                            .yaw = (float)(i % BENCH_SPAWN_DIRS) * (float)MATH_PI / 2};
    }
    return n;
}

static int compare_ms(const void *a, const void *b) {
    double x = *(const double *)a;
    double y = *(const double *)b;
    return (x > y) - (x < y);
}

/* Median render time of one view on the pool, ms */
static double view_ms(const Game *g, Pose view, ThreadPool *pool) {
    Camera cam = camera_from_pose(view, BENCH_FOV);
    double ms[BENCH_REPS];
    for (int r = 0; r < BENCH_REPS; r++) {
        double start = clock_seconds();
        render_scene_lit_mt(&fb, &cam, &g->scene, BENCH_TIME, pool, NULL);
        ms[r] = (clock_seconds() - start) * MS;
    }
    qsort(ms, BENCH_REPS, sizeof(ms[0]), compare_ms);
    return ms[BENCH_REPS / 2];
}

/* Fastest single-thread render of one view, ms */
static double view_solo_ms(const Game *g, Pose view) {
    Camera cam = camera_from_pose(view, BENCH_FOV);
    double best = 0;
    for (int r = 0; r < BENCH_SOLO_REPS; r++) {
        double start = clock_seconds();
        render_scene_lit(&fb, &cam, &g->scene, BENCH_TIME, NULL);
        double ms = (clock_seconds() - start) * MS;
        best = r == 0 || ms < best ? ms : best;
    }
    return best;
}

static int bench_map(const char *path, ThreadPool *pool, BenchResult *out) {
    static Game g; /* large: keep off the stack */
    memset(&g, 0, sizeof(g));
    if (game_init(&g, path) != GAME_LOAD_OK) {
        return -1;
    }

    /* One tick builds the dynamic BVH and sprites, as before any frame */
    PlayerCmd idle = {0};
    game_tick(&g, &idle);

    Pose views[BENCH_VIEWS_MAX];
    int n = bench_views(&g, views);
    double total = 0;
    double worst = 0;
    double solo = 0;
    for (int i = 0; i < n; i++) {
        double ms = view_ms(&g, views[i], pool);
        total += ms;
        worst = ms > worst ? ms : worst;
        solo += view_solo_ms(&g, views[i]);
    }

    *out = (BenchResult){
        .map = path, .tris = g.scene.static_bvh.tri_count + g.scene.dynamic_tri_count,
        .lights = g.scene.lights.count, .sprites = g.scene.sprite_count, .views = n,
        .frame_ms = total / n, .worst_ms = worst, .solo_ms = solo / n,
    };

    double start = clock_seconds();
    for (int i = 0; i < BENCH_TICKS; i++) {
        game_tick(&g, &idle);
    }
    out->tick_ms = (clock_seconds() - start) * MS / BENCH_TICKS;

    game_free(&g);
    return 0;
}

int bench_run(const char *const maps[], int count, FILE *out) {
    if (count == 0) {
        maps = SHIPPED_MAPS;
        count = (int)(sizeof(SHIPPED_MAPS) / sizeof(SHIPPED_MAPS[0]));
    }
    if (count > BENCH_MAPS_MAX) {
        count = BENCH_MAPS_MAX;
    }

    ThreadPool pool;
    threadpool_create(&pool, threadpool_cpu_count());

    /* Loaders print as they go; the table comes after them */
    BenchResult results[BENCH_MAPS_MAX];
    int status = 0;
    for (int m = 0; m < count; m++) {
        if (bench_map(maps[m], &pool, &results[m]) != 0) {
            fprintf(stderr, "bench: can't load %s\n", maps[m]);
            results[m] = (BenchResult){.map = maps[m]};
            status = -1;
        }
    }

    fprintf(out, "\n%d render threads; frame = median of %d renders per view\n",
            pool.worker_count, BENCH_REPS);
    fprintf(out, "%-20s %6s %6s %7s %5s %11s %10s %11s %9s\n", "map", "tris", "lights",
            "sprites", "views", "frame (ms)", "worst (ms)", "1 core (ms)", "tick (ms)");
    double sum = 0;
    double solo = 0;
    for (int m = 0; m < count; m++) {
        const BenchResult *r = &results[m];
        fprintf(out, "%-20s %6d %6d %7d %5d %11.2f %10.2f %11.1f %9.3f\n", r->map, r->tris,
                r->lights, r->sprites, r->views, r->frame_ms, r->worst_ms, r->solo_ms,
                r->tick_ms);
        sum += r->frame_ms;
        solo += r->solo_ms;
    }
    fprintf(out, "%-20s %39.2f %23.1f\n", "mean", sum / count, solo / count);

    threadpool_destroy(&pool);
    return status;
}
