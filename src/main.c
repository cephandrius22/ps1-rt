/*
 * main.c — Wiring: the only file that sees every layer.
 *
 * Per frame:
 *
 *   input_poll() ──PlayerCmd──> game_update() ──events──> play_event_sounds()
 *                                    │
 *                     game_view() ──> camera ──> render_scene_lit_mt()
 *                     Game fields ──> OverlayState ──> overlay_draw()
 *                                                        ──> SDL present
 *
 * The level file is watched: saving it in an editor reloads it in place.
 *
 * Holds no game rules: it only translates between game/, render/ and
 * platform/ (event → sound, game mode → overlay mode). SDL window and
 * present code also live here until Phase 6 moves them to platform/.
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "render.h"
#include "overlay.h"
#include "camera.h"
#include "input.h"
#include "game.h"
#include "threadpool.h"
#include "audio.h"
#include "filewatch.h"
#include "editor_export.h"

#define WINDOW_SCALE 3
#define VIEW_FOV     1.2f /* radians */
#define DEFAULT_LEVEL "levels/e1m1.level"
#define RELOAD_POLL_SEC 0.25f /* how often to check the level file for saves */

/* Presentation side of the event queue: map game events to sounds */
static void play_event_sounds(AudioSystem *audio, const EventQueue *events) {
    for (int i = 0; i < events->count; i++) {
        const GameEvent *ev = &events->items[i];
        switch (ev->type) {
        case EV_WEAPON_FIRED:  audio_play(audio, SND_WEAPON_FIRE, 0.8f);  break;
        case EV_WEAPON_IMPACT: audio_play(audio, SND_WEAPON_IMPACT, 0.5f); break;
        case EV_WEAPON_EMPTY:  audio_play(audio, SND_DRY_FIRE, 0.6f);     break;
        case EV_FOOTSTEP:      audio_play(audio, SND_FOOTSTEP, 0.4f);     break;
        case EV_PICKUP:        audio_play(audio, SND_PICKUP, 0.6f);       break;
        case EV_DOOR_OPENED:   audio_play(audio, SND_DOOR_OPEN, 0.7f);    break;
        case EV_DOOR_CLOSED:   audio_play(audio, SND_DOOR_CLOSE, 0.7f);   break;
        case EV_PLAYER_HURT:   audio_play(audio, SND_PLAYER_PAIN, 0.7f);  break;
        case EV_PLAYER_DIED:   audio_play(audio, SND_PLAYER_DEATH, 0.8f); break;
        case EV_ENEMY_ALERT:   audio_play(audio, SND_ENEMY_ALERT, 0.7f);  break;
        case EV_ENEMY_ATTACK:  audio_play(audio, SND_ENEMY_ATTACK, 0.7f); break;
        case EV_ENEMY_PAIN:    audio_play(audio, SND_ENEMY_PAIN, 0.6f);   break;
        case EV_ENEMY_DIED:    audio_play(audio, SND_ENEMY_DEATH, 0.8f);  break;
        case EV_LEVEL_EXIT:    audio_play(audio, SND_DOOR_OPEN, 0.8f);    break;
        }
    }
}

/* Every RELOAD_POLL_SEC, reload the level if its file was saved */
static void hot_reload(Game *game, FileWatch *watch, float *since_poll, float dt) {
    *since_poll += dt;
    if (*since_poll < RELOAD_POLL_SEC) {
        return;
    }
    *since_poll = 0;

    if (filewatch_poll(watch) != FILE_CHANGED) {
        return;
    }
    if (game_reload(game) == GAME_LOAD_OK) {
        printf("\nReloaded %s\n", game->level_path);
    } else {
        printf("\nReload of %s failed; keeping the current level\n", game->level_path);
    }
}

/* Game mode → what the overlay should show */
static OverlayMode overlay_mode(GameMode mode) {
    switch (mode) {
    case GAME_PLAYING:   return OVERLAY_PLAYING;
    case GAME_DEAD:      return OVERLAY_DEAD;
    case GAME_LEVEL_END: return OVERLAY_LEVEL_END;
    }
    return OVERLAY_PLAYING;
}

int main(int argc, char *argv[]) {
    /* Tool mode: write TrenchBroom textures + FGD, no window */
    if (argc > 2 && strcmp(argv[1], "--export-editor") == 0) {
        return editor_export(argv[2]) == 0 ? 0 : 1;
    }

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

    FileWatch level_watch;
    filewatch_init(&level_watch, level_file);
    float since_poll = 0;

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

        hot_reload(&game, &level_watch, &since_poll, dt);
        game_update(&game, &cmd, dt);
        play_event_sounds(&audio, &game.events);

        /* Render with point lights and shadow rays (multithreaded) */
        Camera cam = camera_from_pose(game_view(&game), VIEW_FOV);
        render_scene_lit_mt(&fb, &cam, &game.scene, game.time, &pool,
                            game.player.flashlight_on);
        OverlayState overlay = {
            .mode = overlay_mode(game.mode),
            .health = (int)ceilf(game.player.health),
            .ammo = game_ammo(&game),
            .kills = game.kills,
            .kills_total = game.kills_total,
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
