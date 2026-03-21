#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include "render.h"
#include "camera.h"
#include "mesh.h"
#include "bvh.h"
#include "input.h"
#include "player.h"

#define WINDOW_SCALE 3

static Triangle make_tri(Vec3 v0, Vec3 v1, Vec3 v2) {
    Vec3 normal = vec3_normalize(vec3_cross(vec3_sub(v1, v0), vec3_sub(v2, v0)));
    return (Triangle){
        .v0 = v0, .v1 = v1, .v2 = v2,
        .uv0 = {0, 0}, .uv1 = {1, 0}, .uv2 = {0, 1},
        .normal = normal, .material_id = 0
    };
}

static void add_quad(Mesh *m, Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
    mesh_add_tri(m, make_tri(a, b, c));
    mesh_add_tri(m, make_tri(a, c, d));
}

static void add_box(Mesh *m, Vec3 lo, Vec3 hi) {
    // Front (z = hi.z)
    add_quad(m, vec3(lo.x, lo.y, hi.z), vec3(hi.x, lo.y, hi.z),
                vec3(hi.x, hi.y, hi.z), vec3(lo.x, hi.y, hi.z));
    // Back (z = lo.z)
    add_quad(m, vec3(hi.x, lo.y, lo.z), vec3(lo.x, lo.y, lo.z),
                vec3(lo.x, hi.y, lo.z), vec3(hi.x, hi.y, lo.z));
    // Left (x = lo.x)
    add_quad(m, vec3(lo.x, lo.y, lo.z), vec3(lo.x, lo.y, hi.z),
                vec3(lo.x, hi.y, hi.z), vec3(lo.x, hi.y, lo.z));
    // Right (x = hi.x)
    add_quad(m, vec3(hi.x, lo.y, hi.z), vec3(hi.x, lo.y, lo.z),
                vec3(hi.x, hi.y, lo.z), vec3(hi.x, hi.y, hi.z));
    // Top (y = hi.y)
    add_quad(m, vec3(lo.x, hi.y, hi.z), vec3(hi.x, hi.y, hi.z),
                vec3(hi.x, hi.y, lo.z), vec3(lo.x, hi.y, lo.z));
    // Bottom (y = lo.y)
    add_quad(m, vec3(lo.x, lo.y, lo.z), vec3(hi.x, lo.y, lo.z),
                vec3(hi.x, lo.y, hi.z), vec3(lo.x, lo.y, hi.z));
}

static void build_test_scene(Mesh *m) {
    mesh_init(m);

    // Floor
    float s = 20.0f;
    add_quad(m,
        vec3(-s, 0, -s), vec3(s, 0, -s),
        vec3(s, 0, s),   vec3(-s, 0, s));

    // Scattered cubes/pillars
    add_box(m, vec3(-1, 0, -6), vec3(1, 2, -4));
    add_box(m, vec3(4, 0, -8), vec3(5.5f, 3, -6.5f));
    add_box(m, vec3(-6, 0, -4), vec3(-4.5f, 1.5f, -2.5f));
    add_box(m, vec3(-3, 0, -12), vec3(-1, 4, -10));
    add_box(m, vec3(2, 0, -14), vec3(4, 2.5f, -12));

    // Walls on the sides
    add_box(m, vec3(-15, 0, -20), vec3(-14, 4, 5));
    add_box(m, vec3(14, 0, -20), vec3(15, 4, 5));
    add_box(m, vec3(-15, 0, -20), vec3(15, 4, -19));

    // Some steps
    add_box(m, vec3(6, 0, -3), vec3(10, 0.5f, -1));
    add_box(m, vec3(7, 0, -3), vec3(10, 1.0f, -1));
    add_box(m, vec3(8, 0, -3), vec3(10, 1.5f, -1));
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "PS1 Raytracer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W * WINDOW_SCALE, SCREEN_H * WINDOW_SCALE,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *sdl_renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl_renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(sdl_renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        SCREEN_W, SCREEN_H);

    Framebuffer fb;
    Player player = player_create(vec3(0, PLAYER_HEIGHT, 3));

    Mesh scene;
    build_test_scene(&scene);
    printf("Scene: %d triangles\n", scene.count);

    // Apply PS1 vertex jitter before building BVH
    for (int i = 0; i < scene.count; i++)
        scene.tris[i] = ps1_jitter_triangle(&scene.tris[i]);

    BVH bvh;
    bvh_build(&bvh, scene.tris, scene.count);
    printf("BVH: %d nodes\n", bvh.node_count);

    input_init();

    bool running = true;
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 last_time = SDL_GetPerformanceCounter();

    while (running) {
        Uint64 frame_start = SDL_GetPerformanceCounter();
        float dt = (float)(frame_start - last_time) / (float)freq;
        last_time = frame_start;
        if (dt > 0.05f) dt = 0.05f; // cap delta time

        InputState input_state;
        input_update(&input_state, &running);
        player_update(&player, &input_state, dt);

        render_scene(&fb, &player.cam, &bvh, NULL);

        SDL_UpdateTexture(texture, NULL, fb.pixels, SCREEN_W * sizeof(uint32_t));
        SDL_RenderClear(sdl_renderer);
        SDL_RenderCopy(sdl_renderer, texture, NULL, NULL);
        SDL_RenderPresent(sdl_renderer);

        Uint64 frame_end = SDL_GetPerformanceCounter();
        float frame_ms = (float)(frame_end - frame_start) / (float)freq * 1000.0f;
        printf("\rFrame: %.1f ms (%.0f fps)", frame_ms, 1000.0f / frame_ms);
        fflush(stdout);
    }

    printf("\n");
    bvh_free(&bvh);
    mesh_free(&scene);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
