#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "render.h"
#include "camera.h"
#include "mesh.h"
#include "bvh.h"
#include "input.h"
#include "player.h"
#include "weapon.h"
#include "scene.h"
#include "level.h"
#include "threadpool.h"
#include "audio.h"

#define WINDOW_SCALE 3

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
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
    Weapon weapon = weapon_create();

    /* Load level */
    Scene scene;
    scene_init(&scene);
    LevelInfo level_info;
    const char *level_file = (argc > 1) ? argv[1] : "levels/e1m1.level";
    if (level_load(&scene, level_file, &level_info) != 0) {
        fprintf(stderr, "Failed to load level: %s\n", level_file);
        scene_free(&scene);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(sdl_renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Player player = player_create(vec3(
        level_info.spawn_pos.x,
        level_info.spawn_pos.y + PLAYER_HEIGHT + PLAYER_EYE_OFFSET,
        level_info.spawn_pos.z));
    player.cam.yaw = level_info.spawn_yaw * ((float)M_PI / 180.0f);

    input_init();

    /* Create thread pool for parallel rendering */
    int num_threads = threadpool_cpu_count();
    ThreadPool pool;
    threadpool_create(&pool, num_threads);
    printf("Render threads: %d\n", num_threads);

    /* Initialize audio */
    AudioSystem audio;
    if (audio_init(&audio) == 0) {
        printf("Audio initialized\n");
        audio_play_loop(&audio, SND_AMBIENT, 0.3f);
    } else {
        printf("Audio init failed (continuing without sound)\n");
    }

    #define FOOTSTEP_INTERVAL 2.5f  /* distance between footstep sounds */

    bool running = true;
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 last_time = SDL_GetPerformanceCounter();
    float game_time = 0;

    while (running) {
        Uint64 frame_start = SDL_GetPerformanceCounter();
        float dt = (float)(frame_start - last_time) / (float)freq;
        last_time = frame_start;
        if (dt > 0.05f) dt = 0.05f;
        game_time += dt;

        InputState input_state;
        input_update(&input_state, &running);

        /* Update entities (bobbing, door animation) */
        entity_update(&scene.entities, dt, game_time);

        /* Update animated lights (swinging bulbs, etc.) */
        light_update(&scene.lights, game_time);

        /* Rebuild dynamic BVH from entity geometry */
        scene_rebuild_dynamic(&scene);

        /* Update player and weapon using full scene intersection */
        player_update_scene(&player, &input_state, dt, &scene);
        weapon_update_scene(&weapon, &input_state, &player.cam, &scene, dt);

        /* Audio triggers */
        if (weapon.fired_this_frame) {
            audio_play(&audio, SND_WEAPON_FIRE, 0.8f);
            if (weapon.last_hit.hit)
                audio_play(&audio, SND_WEAPON_IMPACT, 0.5f);
        }
        if (player.walk_distance >= FOOTSTEP_INTERVAL) {
            player.walk_distance -= FOOTSTEP_INTERVAL;
            audio_play(&audio, SND_FOOTSTEP, 0.4f);
        }
        if (player.collected > 0)
            audio_play(&audio, SND_PICKUP, 0.6f);
        if (player.door_action == 1)
            audio_play(&audio, SND_DOOR_OPEN, 0.7f);
        else if (player.door_action == -1)
            audio_play(&audio, SND_DOOR_CLOSE, 0.7f);

        /* Render with point lights and shadow rays (multithreaded) */
        render_scene_lit_mt(&fb, &player.cam, &scene, game_time, &pool, player.flashlight_on);
        weapon_draw_crosshair(fb.pixels, SCREEN_W, SCREEN_H);
        weapon_draw_viewmodel(fb.pixels, SCREEN_W, SCREEN_H, &weapon);

        SDL_UpdateTexture(texture, NULL, fb.pixels, SCREEN_W * sizeof(uint32_t));
        SDL_RenderClear(sdl_renderer);
        SDL_RenderCopy(sdl_renderer, texture, NULL, NULL);
        SDL_RenderPresent(sdl_renderer);

        Uint64 frame_end = SDL_GetPerformanceCounter();
        float frame_ms = (float)(frame_end - frame_start) / (float)freq * 1000.0f;
        printf("\rFrame: %.1f ms (%.0f fps)  Entities: %d  Lights: %d",
               frame_ms, 1000.0f / frame_ms, scene.entities.count, scene.lights.count);
        fflush(stdout);
    }

    printf("\n");
    audio_shutdown(&audio);
    threadpool_destroy(&pool);
    scene_free(&scene);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
