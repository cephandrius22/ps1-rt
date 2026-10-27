#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "render.h"
#include "overlay.h"
#include "camera.h"
#include "input.h"
#include "game.h"
#include "threadpool.h"
#include "audio.h"

#define WINDOW_SCALE 3
#define VIEW_FOV     1.2f /* radians */
#define DEFAULT_LEVEL "levels/e1m1.level"

/* Presentation side of the event queue: map game events to sounds */
static void play_event_sounds(AudioSystem *audio, const EventQueue *events) {
    for (int i = 0; i < events->count; i++) {
        const GameEvent *ev = &events->items[i];
        switch (ev->type) {
        case EV_WEAPON_FIRED:  audio_play(audio, SND_WEAPON_FIRE, 0.8f);  break;
        case EV_WEAPON_IMPACT: audio_play(audio, SND_WEAPON_IMPACT, 0.5f); break;
        case EV_WEAPON_EMPTY:  break; /* dry-fire click arrives with audio step */
        case EV_FOOTSTEP:      audio_play(audio, SND_FOOTSTEP, 0.4f);     break;
        case EV_PICKUP:        audio_play(audio, SND_PICKUP, 0.6f);       break;
        case EV_DOOR_OPENED:   audio_play(audio, SND_DOOR_OPEN, 0.7f);    break;
        case EV_DOOR_CLOSED:   audio_play(audio, SND_DOOR_CLOSE, 0.7f);   break;
        case EV_PLAYER_HURT:   break; /* sounds arrive with enemy audio */
        case EV_PLAYER_DIED:   break;
        case EV_ENEMY_ALERT:   break;
        case EV_ENEMY_ATTACK:  break;
        case EV_ENEMY_PAIN:    break;
        case EV_ENEMY_DIED:    break;
        }
    }
}

int main(int argc, char *argv[]) {
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

    /* Game owns the level, player and weapon (static: Scene is large) */
    static Game game;
    const char *level_file = (argc > 1) ? argv[1] : DEFAULT_LEVEL;
    if (game_init(&game, level_file) != GAME_LOAD_OK) {
        fprintf(stderr, "Failed to load level: %s\n", level_file);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(sdl_renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

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

    bool running = true;
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 last_time = SDL_GetPerformanceCounter();

    while (running) {
        Uint64 frame_start = SDL_GetPerformanceCounter();
        float dt = (float)(frame_start - last_time) / (float)freq;
        last_time = frame_start;

        PlayerCmd cmd;
        if (input_poll(&cmd) == INPUT_QUIT) {
            running = false;
        }

        game_update(&game, &cmd, dt);
        play_event_sounds(&audio, &game.events);

        /* Render with point lights and shadow rays (multithreaded) */
        Camera cam = camera_from_pose(game_view(&game), VIEW_FOV);
        render_scene_lit_mt(&fb, &cam, &game.scene, game.time, &pool,
                            game.player.flashlight_on);
        OverlayState overlay = {
            .mode = game.mode == GAME_DEAD ? OVERLAY_DEAD : OVERLAY_PLAYING,
            .health = (int)ceilf(game.player.health),
            .ammo = game_ammo(&game),
            .weapon_kick = game.weapon.kick,
        };
        overlay_draw(&fb, &overlay);

        SDL_UpdateTexture(texture, NULL, fb.pixels, SCREEN_W * sizeof(uint32_t));
        SDL_RenderClear(sdl_renderer);
        SDL_RenderCopy(sdl_renderer, texture, NULL, NULL);
        SDL_RenderPresent(sdl_renderer);

        Uint64 frame_end = SDL_GetPerformanceCounter();
        float frame_ms = (float)(frame_end - frame_start) / (float)freq * 1000.0f;
        printf("\rFrame: %.1f ms (%.0f fps)  Entities: %d  Lights: %d",
               frame_ms, 1000.0f / frame_ms, game.scene.entities.count, game.scene.lights.count);
        fflush(stdout);
    }

    printf("\n");
    audio_shutdown(&audio);
    threadpool_destroy(&pool);
    game_free(&game);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
