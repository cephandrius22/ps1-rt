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
#include "threadpool.h"
#include "audio.h"

#define WINDOW_SCALE 3

static Triangle make_tri(Vec3 v0, Vec3 v1, Vec3 v2, int mat_id) {
    Vec3 normal = vec3_normalize(vec3_cross(vec3_sub(v1, v0), vec3_sub(v2, v0)));
    return (Triangle){
        .v0 = v0, .v1 = v1, .v2 = v2,
        .uv0 = {0, 0}, .uv1 = {1, 0}, .uv2 = {0, 1},
        .normal = normal, .material_id = mat_id
    };
}

static void add_quad(Mesh *m, Vec3 a, Vec3 b, Vec3 c, Vec3 d, int mat_id) {
    mesh_add_tri(m, make_tri(a, b, c, mat_id));
    mesh_add_tri(m, make_tri(a, c, d, mat_id));
}

static void add_box(Mesh *m, Vec3 lo, Vec3 hi, int mat_id) {
    add_quad(m, vec3(lo.x, lo.y, hi.z), vec3(hi.x, lo.y, hi.z),
                vec3(hi.x, hi.y, hi.z), vec3(lo.x, hi.y, hi.z), mat_id);
    add_quad(m, vec3(hi.x, lo.y, lo.z), vec3(lo.x, lo.y, lo.z),
                vec3(lo.x, hi.y, lo.z), vec3(hi.x, hi.y, lo.z), mat_id);
    add_quad(m, vec3(lo.x, lo.y, lo.z), vec3(lo.x, lo.y, hi.z),
                vec3(lo.x, hi.y, hi.z), vec3(lo.x, hi.y, lo.z), mat_id);
    add_quad(m, vec3(hi.x, lo.y, hi.z), vec3(hi.x, lo.y, lo.z),
                vec3(hi.x, hi.y, lo.z), vec3(hi.x, hi.y, hi.z), mat_id);
    add_quad(m, vec3(lo.x, hi.y, hi.z), vec3(hi.x, hi.y, hi.z),
                vec3(hi.x, hi.y, lo.z), vec3(lo.x, hi.y, lo.z), mat_id);
    add_quad(m, vec3(lo.x, lo.y, lo.z), vec3(hi.x, lo.y, lo.z),
                vec3(hi.x, lo.y, hi.z), vec3(lo.x, lo.y, hi.z), mat_id);
}

/* Material IDs */
enum {
    MAT_FLOOR = 0,
    MAT_WALL,
    MAT_PILLAR,
    MAT_HEALTH,
    MAT_AMMO,
    MAT_DOOR,
    MAT_COUNT
};

static void build_test_scene(Mesh *m) {
    mesh_init(m);

    float s = 20.0f;
    add_quad(m,
        vec3(-s, 0, -s), vec3(s, 0, -s),
        vec3(s, 0, s),   vec3(-s, 0, s), MAT_FLOOR);

    add_box(m, vec3(-1, 0, -6), vec3(1, 2, -4), MAT_PILLAR);
    add_box(m, vec3(4, 0, -8), vec3(5.5f, 3, -6.5f), MAT_PILLAR);
    add_box(m, vec3(-6, 0, -4), vec3(-4.5f, 1.5f, -2.5f), MAT_PILLAR);
    add_box(m, vec3(-3, 0, -12), vec3(-1, 4, -10), MAT_PILLAR);
    add_box(m, vec3(2, 0, -14), vec3(4, 2.5f, -12), MAT_PILLAR);

    add_box(m, vec3(-15, 0, -20), vec3(-14, 4, 5), MAT_WALL);
    add_box(m, vec3(14, 0, -20), vec3(15, 4, 5), MAT_WALL);
    add_box(m, vec3(-15, 0, -20), vec3(15, 4, -19), MAT_WALL);

    add_box(m, vec3(6, 0, -3), vec3(10, 0.5f, -1), MAT_PILLAR);
    add_box(m, vec3(7, 0, -3), vec3(10, 1.0f, -1), MAT_PILLAR);
    add_box(m, vec3(8, 0, -3), vec3(10, 1.5f, -1), MAT_PILLAR);
}

static void setup_materials(Scene *scene) {
    scene->material_count = MAT_COUNT;
    scene->materials = malloc((size_t)MAT_COUNT * sizeof(Material));

    scene->materials[MAT_FLOOR]  = (Material){.color = vec3(0.4f, 0.35f, 0.3f), .texture_id = -1};
    scene->materials[MAT_WALL]   = (Material){.color = vec3(0.5f, 0.45f, 0.4f), .texture_id = -1};
    scene->materials[MAT_PILLAR] = (Material){.color = vec3(0.55f, 0.5f, 0.45f), .texture_id = -1};
    scene->materials[MAT_HEALTH] = (Material){.color = vec3(0.9f, 0.2f, 0.2f), .texture_id = -1};
    scene->materials[MAT_AMMO]   = (Material){.color = vec3(0.9f, 0.8f, 0.2f), .texture_id = -1};
    scene->materials[MAT_DOOR]   = (Material){.color = vec3(0.5f, 0.3f, 0.15f), .texture_id = -1};
}

static void setup_entities(Scene *scene) {
    /* Health pickups */
    Entity health1 = {
        .type = ENTITY_PICKUP_HEALTH, .position = vec3(3, 0.5f, -3),
        .rotation_y = 0, .active = true, .material_id = MAT_HEALTH,
        .mesh_id = ENTITY_PICKUP_HEALTH, .pickup = {.amount = 25}
    };
    Entity health2 = {
        .type = ENTITY_PICKUP_HEALTH, .position = vec3(-5, 0.5f, -8),
        .rotation_y = 0, .active = true, .material_id = MAT_HEALTH,
        .mesh_id = ENTITY_PICKUP_HEALTH, .pickup = {.amount = 50}
    };

    /* Ammo pickups */
    Entity ammo1 = {
        .type = ENTITY_PICKUP_AMMO, .position = vec3(-2, 0.5f, -2),
        .rotation_y = 0, .active = true, .material_id = MAT_AMMO,
        .mesh_id = ENTITY_PICKUP_AMMO, .pickup = {.amount = 10}
    };

    /* Door */
    Entity door1 = {
        .type = ENTITY_DOOR, .position = vec3(0, 0, -8),
        .rotation_y = 0, .active = true, .material_id = MAT_DOOR,
        .mesh_id = ENTITY_DOOR,
        .door = {.angle = 0, .target_angle = 0, .swing_speed = DOOR_SWING_SPEED,
                 .hinge = vec3(0, 0, -8)}
    };

    entity_add(&scene->entities, health1);
    entity_add(&scene->entities, health2);
    entity_add(&scene->entities, ammo1);
    entity_add(&scene->entities, door1);
}

static void setup_lights(Scene *scene) {
    /* Warm overhead light near spawn */
    light_add(&scene->lights, (PointLight){
        .position = vec3(0, 3.5f, 1), .color = vec3(1.0f, 0.9f, 0.7f),
        .intensity = 3.0f, .radius = 20.0f, .cast_shadows = true,
        .flicker_speed = 0, .flicker_amount = 0
    });

    /* Flickering red light near health pickup */
    light_add(&scene->lights, (PointLight){
        .position = vec3(3, 2.5f, -3), .color = vec3(1.0f, 0.3f, 0.2f),
        .intensity = 2.5f, .radius = 12.0f, .cast_shadows = true,
        .flicker_speed = 3.0f, .flicker_amount = 0.4f
    });

    /* Blue light deep in the scene */
    light_add(&scene->lights, (PointLight){
        .position = vec3(-2, 3.0f, -11), .color = vec3(0.3f, 0.4f, 1.0f),
        .intensity = 3.0f, .radius = 16.0f, .cast_shadows = true,
        .flicker_speed = 0, .flicker_amount = 0
    });

    /* Yellow light near ammo */
    light_add(&scene->lights, (PointLight){
        .position = vec3(-2, 2.0f, -2), .color = vec3(1.0f, 0.9f, 0.4f),
        .intensity = 2.0f, .radius = 10.0f, .cast_shadows = false,
        .flicker_speed = 0, .flicker_amount = 0
    });

    /* Flickering torch light near steps */
    light_add(&scene->lights, (PointLight){
        .position = vec3(7, 2.5f, -2), .color = vec3(1.0f, 0.6f, 0.2f),
        .intensity = 3.0f, .radius = 14.0f, .cast_shadows = true,
        .flicker_speed = 5.0f, .flicker_amount = 0.3f
    });

    /* Swinging bare bulb in the corridor — creepy pendulum */
    light_add(&scene->lights, (PointLight){
        .position = vec3(0, 2.5f, -8), .color = vec3(1.0f, 0.85f, 0.5f),
        .intensity = 3.5f, .radius = 14.0f, .cast_shadows = true,
        .flicker_speed = 8.0f, .flicker_amount = 0.15f,
        .anchor = vec3(0, 3.8f, -8), .cord_length = 1.3f,
        .swing_speed = 2.0f, .swing_angle = 0.35f
    });

    /* Slow-swinging green light deep in the back — eerie */
    light_add(&scene->lights, (PointLight){
        .position = vec3(-5, 2.0f, -15), .color = vec3(0.2f, 1.0f, 0.3f),
        .intensity = 2.5f, .radius = 14.0f, .cast_shadows = true,
        .flicker_speed = 0, .flicker_amount = 0,
        .anchor = vec3(-5, 3.5f, -15), .cord_length = 1.5f,
        .swing_speed = 1.2f, .swing_angle = 0.5f
    });

    /* Fast-swinging red warning light near wall */
    light_add(&scene->lights, (PointLight){
        .position = vec3(10, 2.5f, -10), .color = vec3(1.0f, 0.1f, 0.05f),
        .intensity = 3.0f, .radius = 16.0f, .cast_shadows = true,
        .flicker_speed = 6.0f, .flicker_amount = 0.5f,
        .anchor = vec3(10, 3.5f, -10), .cord_length = 1.0f,
        .swing_speed = 3.5f, .swing_angle = 0.25f
    });
}

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
    Player player = player_create(vec3(0, PLAYER_HEIGHT + PLAYER_EYE_OFFSET, 3));
    Weapon weapon = weapon_create();

    /* Build scene */
    Scene scene;
    scene_init(&scene);
    setup_materials(&scene);
    setup_entities(&scene);
    setup_lights(&scene);

    Mesh world_mesh;
    build_test_scene(&world_mesh);
    printf("Scene: %d static triangles\n", world_mesh.count);

    /* Apply PS1 vertex jitter to static geometry */
    for (int i = 0; i < world_mesh.count; i++)
        world_mesh.tris[i] = ps1_jitter_triangle(&world_mesh.tris[i]);

    scene_build_static(&scene, world_mesh.tris, world_mesh.count);
    printf("Static BVH: %d nodes\n", scene.static_bvh.node_count);

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
        render_scene_lit_mt(&fb, &player.cam, &scene, game_time, &pool);
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
    mesh_free(&world_mesh);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
