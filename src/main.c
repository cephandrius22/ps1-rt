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
#include <stdarg.h>
#include "render.h"
#include "overlay.h"
#include "camera.h"
#include "input.h"
#include "game.h"
#include "threadpool.h"
#include "audio.h"
#include "filewatch.h"
#include "editor_export.h"
#include "screenfx.h"
#include "levelcheck.h"
#include "mapfile.h"

#define WINDOW_SCALE 3
#define VIEW_FOV     1.2f /* radians */
#define DEFAULT_LEVEL "levels/e1m1.level"
#define RELOAD_POLL_SEC 0.25f /* how often to check the level file for saves */
#define FRAME_BUDGET_MS 16.7f /* 60 fps */
#define WARN_FRACTION   0.8f  /* debug values turn yellow at 80% of their limit */
#define MS_SMOOTHING    0.1f  /* weight of the newest frame in debug averages */
#define DEG_PER_RAD     (180.0f / (float)M_PI)
#define FULL_TURN_DEG   360.0f

/* Presentation side of the event queue: map game events to sounds */
#define HURT_FLASH_PER_HP 0.03f  /* flash strength per point of damage taken */
#define HURT_SHAKE        0.012f /* camera jolt when hit, radians */
#define HURT_COLOR        vec3(0.8f, 0.05f, 0.02f)

/* Presentation side of the event queue: camera jolts and color flashes */
static void play_event_effects(ScreenFx *fx, const EventQueue *events, const Game *g) {
    for (int i = 0; i < events->count; i++) {
        const GameEvent *ev = &events->items[i];
        switch (ev->type) {
        case EV_PLAYER_HURT:
        case EV_PLAYER_DIED:
            screenfx_flash(fx, HURT_COLOR, ev->amount * HURT_FLASH_PER_HP);
            screenfx_shake(fx, HURT_SHAKE);
            break;
        case EV_WEAPON_FIRED:
            screenfx_shake(fx, weapon_def(g->weapon.type)->shake);
            break;
        default:
            break;
        }
    }
}

static void play_event_sounds(AudioSystem *audio, const EventQueue *events) {
    for (int i = 0; i < events->count; i++) {
        const GameEvent *ev = &events->items[i];
        switch (ev->type) {
        case EV_WEAPON_FIRED:  audio_play(audio, SND_WEAPON_FIRE, 0.8f);  break;
        case EV_WEAPON_IMPACT: audio_play(audio, SND_WEAPON_IMPACT, 0.5f); break;
        case EV_WEAPON_EMPTY:  audio_play(audio, SND_DRY_FIRE, 0.6f);     break;
        case EV_FOOTSTEP:      audio_play(audio, SND_FOOTSTEP, 0.4f);     break;
        case EV_PICKUP:        audio_play(audio, SND_PICKUP, 0.6f);       break;
        case EV_WEAPON_PICKUP: break; /* EV_PICKUP already plays the pickup sound */
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

static DebugSeverity severity(float value, float limit) {
    if (value > limit) {
        return DEBUG_OVER;
    }
    return value >= limit * WARN_FRACTION ? DEBUG_WARN : DEBUG_OK;
}

static void debug_line(OverlayState *o, DebugSeverity sev, const char *fmt, ...) {
    if (o->debug_count >= OVERLAY_DEBUG_LINES) {
        return;
    }
    DebugLine *line = &o->debug[o->debug_count++];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line->text, sizeof(line->text), fmt, args);
    va_end(args);
    line->severity = sev;
}

/* F3 overlay: work time per frame (excluding vsync wait), budget use,
 * and where the player stands in meters and TrenchBroom units/angle */
static void fill_debug(OverlayState *o, const Game *g, float work_ms, float fps) {
    const Scene *s = &g->scene;
    debug_line(o, severity(work_ms, FRAME_BUDGET_MS), "%.1f MS %.0f FPS", work_ms, fps);
    debug_line(o, severity((float)s->static_bvh.tri_count, BUDGET_TRIANGLES),
               "TRI %d/%d", s->static_bvh.tri_count, BUDGET_TRIANGLES);
    debug_line(o, severity((float)s->lights.count, BUDGET_LIGHTS),
               "LIT %d/%d", s->lights.count, BUDGET_LIGHTS);
    debug_line(o, severity((float)s->sprite_count, BUDGET_SPRITES),
               "SPR %d/%d", s->sprite_count, BUDGET_SPRITES);

    const Player *p = &g->player;
    float k = MAP_UNITS_PER_METER;
    float angle = fmodf(p->view.yaw * DEG_PER_RAD + 90.0f, FULL_TURN_DEG); /* map "angle" */
    angle = angle < 0 ? angle + FULL_TURN_DEG : angle;
    debug_line(o, DEBUG_OK, "POS %.1f %.1f %.1f MAP %.0f %.0f %.0f A %.0f",
               p->view.pos.x, p->foot_y, p->view.pos.z,
               p->view.pos.x * k, -p->view.pos.z * k, p->foot_y * k, angle);
}

/* Viewmodel animation: presentation only, driven by frame time */
#define BOB_RATE          9.0f   /* bob phase speed (rad/s) at full walking speed */
#define BOB_X             6.0f   /* sway, pixels */
#define BOB_Y             4.0f   /* dip, pixels */
#define VIEW_FIRE_SHOW    0.07f  /* seconds the firing frame stays up */
#define RECOIL_PX_PER_RAD 150.0f /* how far the gun jumps per radian of kick */
#define VIEW_LOWER_PX     110.0f /* drop when fully lowered for a switch */
#define VIEW_LIGHT_SCALE  0.45f  /* probe ignores shadows and angles: tone it down */
#define VIEW_LIGHT_MIN    0.25f  /* never fully black */
#define VIEW_LIGHT_MAX    1.1f

typedef struct {
    float bob_phase;
    float fire_show; /* seconds left showing the firing frame */
} ViewAnim;

static bool fired_this_frame(const EventQueue *events) {
    for (int i = 0; i < events->count; i++) {
        if (events->items[i].type == EV_WEAPON_FIRED) {
            return true;
        }
    }
    return false;
}

/* The held weapon: which sheet and frame, bob/recoil/switch offsets, and
 * a brightness probe so the gun is dark in dark rooms */
static void fill_viewmodel(ViewmodelState *vm, ViewAnim *anim, const Game *g, float dt) {
    if (g->mode != GAME_PLAYING) {
        vm->texture = NULL;
        return;
    }

    const Player *p = &g->player;
    float walk = p->on_ground ? fminf(p->move_speed / PLAYER_SPEED, 1.0f) : 0.0f;
    anim->bob_phase += dt * BOB_RATE * walk;
    anim->fire_show = fired_this_frame(&g->events) ? VIEW_FIRE_SHOW
                                                   : fmaxf(anim->fire_show - dt, 0.0f);

    vm->texture = weapon_def(g->weapon.type)->view_texture;
    vm->frame = anim->fire_show > 0 ? VIEW_FRAME_FIRE : VIEW_FRAME_IDLE;
    vm->offset_x = sinf(anim->bob_phase) * BOB_X * walk;
    vm->offset_y = fabsf(cosf(anim->bob_phase)) * BOB_Y * walk
                 + g->weapon.kick * RECOIL_PX_PER_RAD
                 + weapon_lowered(&g->weapon) * VIEW_LOWER_PX;

    float light = render_light_at(&g->scene, p->view.pos, g->time, p->flashlight_on);
    vm->light = fminf(fmaxf(light * VIEW_LIGHT_SCALE, VIEW_LIGHT_MIN), VIEW_LIGHT_MAX);
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
    Overlay hud;
    overlay_init(&hud);
    ViewAnim view_anim = {0};
    ScreenFx fx = {0};

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
    bool show_debug = false;
    float work_ms = 0;   /* smoothed */
    float fps = 0;       /* smoothed */
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 last_time = SDL_GetPerformanceCounter();

    while (running) {
        Uint64 frame_start = SDL_GetPerformanceCounter();
        float dt = (float)(frame_start - last_time) / (float)freq;
        last_time = frame_start;

        PlayerCmd cmd;
        UiCmd ui;
        if (input_poll(&cmd, &ui) == INPUT_QUIT) {
            running = false;
        }
        if (ui.toggle_debug) {
            show_debug = !show_debug;
        }

        hot_reload(&game, &level_watch, &since_poll, dt);
        game_update(&game, &cmd, dt);
        play_event_sounds(&audio, &game.events);
        play_event_effects(&fx, &game.events, &game);
        screenfx_update(&fx, dt);

        /* Render with point lights and shadow rays (multithreaded) */
        /* Shake moves only what we see, never where shots go */
        Pose view = game_view(&game);
        float shake_yaw, shake_pitch;
        screenfx_camera_offset(&fx, game.time, &shake_yaw, &shake_pitch);
        view.yaw += shake_yaw;
        view.pitch += shake_pitch;
        Camera cam = camera_from_pose(view, VIEW_FOV);
        render_scene_lit_mt(&fb, &cam, &game.scene, game.time, &pool,
                            game.player.flashlight_on);
        OverlayState overlay = {
            .mode = overlay_mode(game.mode),
            .health = (int)ceilf(game.player.health),
            .ammo = game_ammo(&game),
            .kills = game.kills,
            .kills_total = game.kills_total,
            .weapon_kick = game.weapon.kick,
            .flash = fx.flash,
            .flash_color = fx.flash_color,
        };
        fill_viewmodel(&overlay.viewmodel, &view_anim, &game, dt);
        if (show_debug) {
            fill_debug(&overlay, &game, work_ms, fps);
        }
        overlay_draw(&hud, &fb, &overlay);

        /* Work time: everything this frame except waiting for vsync */
        float frame_work = (float)(SDL_GetPerformanceCounter() - frame_start) / (float)freq * 1000.0f;
        work_ms += (frame_work - work_ms) * MS_SMOOTHING;
        fps += ((dt > 0 ? 1.0f / dt : 0) - fps) * MS_SMOOTHING;

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
    overlay_free(&hud);
    game_free(&game);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
