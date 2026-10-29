/*
 * main.c — Wiring: the only file that sees every layer.
 *
 * Per frame:
 *
 *   input_poll() ──PlayerCmd──> game_update() ──events──> play_event_sounds()
 *                                    │                     (placed by play_at)
 *                     game_view() ──> camera ──> fade_view_env() (zone fog,
 *                                    │           reverb, ambience loop)
 *                                    └──> render_scene_lit_mt(RenderView)
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
#include "particles.h"
#include "levelcheck.h"
#include "mapfile.h"

#define WINDOW_SCALE 3
#define VIEW_FOV     1.2f /* radians */
#define DEFAULT_LEVEL "levels/e1m1.level"
#define RELOAD_POLL_SEC 0.25f /* how often to check the level file for saves */
#define FRAME_BUDGET_MS 16.7f /* 60 fps */
#define WARN_FRACTION   0.8f  /* debug values turn yellow at 80% of their limit */
#define MS_SMOOTHING    0.1f  /* weight of the newest frame in debug averages */
#define DEG_PER_RAD     (180.0f / (float)MATH_PI)
#define FULL_TURN_DEG   360.0f

/* Presentation side of the event queue: map game events to sounds */
#define HURT_FLASH_PER_HP 0.03f  /* flash strength per point of damage taken */
#define HURT_SHAKE        0.012f /* camera jolt when hit, radians */
#define HURT_COLOR        vec3(0.8f, 0.05f, 0.02f)
#define BLAST_SHAKE       0.06f  /* camera jolt at the center of an explosion */
#define BLAST_SHAKE_RANGE 14.0f  /* felt out to this distance, meters */

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
        case EV_EXPLOSION: {
            /* Jolt fades with distance from the blast */
            float dist = vec3_length(vec3_sub(ev->pos, g->player.view.pos));
            screenfx_shake(fx, BLAST_SHAKE * fmaxf(0.0f, 1.0f - dist / BLAST_SHAKE_RANGE));
            break;
        }
        default:
            break;
        }
    }
}

/* Particle bursts per event: count, speed (m/s), spread (0..1) */
#define SPARK_COUNT   6
#define SPARK_SPEED   2.5f
#define BLOOD_COUNT   10
#define BLOOD_SPEED   2.0f
#define GIB_COUNT     10
#define GIB_SPEED     3.0f
#define GIB_BLOOD     16
#define BODY_CENTER   0.9f  /* gibs burst from this high above an enemy's feet */
#define DEBRIS_COUNT  24
#define DEBRIS_SPEED  4.0f
#define BLAST_SPARKS  16
#define SPIT_SPLASH   10

/* Presentation side of the event queue: blood, gibs and sparks. Particles
 * land at the player's floor height (levels are mostly flat). */
static void spawn_event_particles(ParticleSystem *ps, const EventQueue *events, const Game *g) {
    Vec3 eye = g->player.view.pos;
    float floor_y = g->player.foot_y;

    for (int i = 0; i < events->count; i++) {
        const GameEvent *ev = &events->items[i];
        Vec3 toward_eye = vec3_sub(eye, ev->pos);

        bool wall_hit = ev->type == EV_WEAPON_IMPACT && ev->entity == ENTITY_INDEX_NONE;
        if (wall_hit || ev->type == EV_LIGHT_SHOT) {
            particles_burst(ps, PARTICLE_SPARK, ev->pos, toward_eye,
                            SPARK_COUNT, SPARK_SPEED, 0.8f, floor_y);
        } else if (ev->type == EV_WEAPON_IMPACT) {
            particles_burst(ps, PARTICLE_BLOOD, ev->pos, vec3_mul(toward_eye, -1),
                            BLOOD_COUNT, BLOOD_SPEED, 0.6f, floor_y);
        } else if (ev->type == EV_EXPLOSION && ev->amount == 0) {
            particles_burst(ps, PARTICLE_SPIT, ev->pos, toward_eye, SPIT_SPLASH,
                            SPARK_SPEED, 1.0f, floor_y); /* no splash radius: a splat */
        } else if (ev->type == EV_EXPLOSION) {
            particles_burst(ps, PARTICLE_DEBRIS, ev->pos, vec3(0, 1, 0), DEBRIS_COUNT,
                            DEBRIS_SPEED, 1.0f, floor_y);
            particles_burst(ps, PARTICLE_SPARK, ev->pos, vec3(0, 1, 0), BLAST_SPARKS,
                            DEBRIS_SPEED * 2, 1.0f, floor_y);
        } else if (ev->type == EV_ENEMY_DIED) {
            Vec3 body = vec3_add(ev->pos, vec3(0, BODY_CENTER, 0));
            particles_burst(ps, PARTICLE_GIB, body, vec3(0, 1, 0), GIB_COUNT, GIB_SPEED, 1.0f, floor_y);
            particles_burst(ps, PARTICLE_BLOOD, body, vec3(0, 1, 0), GIB_BLOOD, GIB_SPEED, 1.0f, floor_y);
        }
    }
}

/* Fire sound per weapon (platform sound IDs can't live in WeaponDef) */
static const SoundID weapon_fire_sounds[WEAPON_TYPE_COUNT] = {
    [WEAPON_PISTOL]   = SND_WEAPON_FIRE,
    [WEAPON_SHOTGUN]  = SND_SHOTGUN_FIRE,
    [WEAPON_LAUNCHER] = SND_ROCKET_LAUNCH,
};

/* Voice per enemy type, indexed by EntityType */
typedef struct {
    SoundID alert, attack, pain, death;
} EnemySounds;

static const EnemySounds enemy_sounds[ENTITY_TYPE_COUNT] = {
    [ENTITY_GHOUL]   = {SND_ENEMY_ALERT, SND_ENEMY_ATTACK, SND_ENEMY_PAIN, SND_ENEMY_DEATH},
    [ENTITY_SPITTER] = {SND_SPITTER_ALERT, SND_SPITTER_ATTACK, SND_SPITTER_PAIN, SND_SPITTER_DEATH},
};

static const EnemySounds *sounds_of(const Game *g, int entity) {
    return &enemy_sounds[g->scene.entities.items[entity].type];
}

/* Rockets boom; spit splats (the event's entity is the spent projectile) */
static SoundID burst_sound(const Game *g, int projectile) {
    int kind = g->scene.entities.items[projectile].projectile.kind;
    return kind == PROJECTILE_SPIT ? SND_SPIT_SPLAT : SND_EXPLOSION;
}

/* --- Sound in space: louder near, panned toward where it happened --- */

#define NEAR_FIELD  2.0f /* meters: full volume, centered (your own steps, shots) */
#define HEARING_REF 6.0f /* meters past the near field to half volume */

static void play_at(AudioSystem *audio, SoundID id, float volume, Pose ear, Vec3 pos) {
    Vec3 to = vec3_sub(pos, ear.pos);
    Vec3 flat = vec3(to.x, 0, to.z);
    float dist = vec3_length(to);
    float far = fmaxf(dist - NEAR_FIELD, 0.0f);
    float gain = 1.0f / (1.0f + far / HEARING_REF);

    /* Right of the view (yaw 0 faces -Z, so right is +X) */
    Vec3 right = vec3(cosf(ear.yaw), 0, -sinf(ear.yaw));
    float flat_len = vec3_length(flat);
    float pan = flat_len > 1e-3f ? vec3_dot(flat, right) / flat_len : 0;
    pan *= fminf(dist / NEAR_FIELD, 1.0f); /* close sounds stay centered */

    audio_play_at(audio, id, volume * gain, pan);
}

/* Presentation side of the event queue: one sound per event, heard
 * from where the player stands */
static void play_event_sounds(AudioSystem *audio, const EventQueue *events, const Game *g) {
    Pose ear = g->player.view;
    for (int i = 0; i < events->count; i++) {
        const GameEvent *ev = &events->items[i];
        switch (ev->type) {
        case EV_WEAPON_FIRED:
            play_at(audio, weapon_fire_sounds[g->weapon.type], 0.8f, ear, ev->pos);
            break;
        case EV_EXPLOSION:     play_at(audio, burst_sound(g, ev->entity), 0.9f, ear, ev->pos); break;
        case EV_WEAPON_IMPACT: play_at(audio, SND_WEAPON_IMPACT, 0.5f, ear, ev->pos); break;
        case EV_WEAPON_EMPTY:  play_at(audio, SND_DRY_FIRE, 0.6f, ear, ev->pos);     break;
        case EV_FOOTSTEP:      play_at(audio, SND_FOOTSTEP, 0.4f, ear, ev->pos);     break;
        case EV_PICKUP:        play_at(audio, SND_PICKUP, 0.6f, ear, ev->pos);       break;
        case EV_WEAPON_PICKUP: break; /* EV_PICKUP already plays the pickup sound */
        case EV_DOOR_OPENED:   play_at(audio, SND_DOOR_OPEN, 0.7f, ear, ev->pos);    break;
        case EV_DOOR_CLOSED:   play_at(audio, SND_DOOR_CLOSE, 0.7f, ear, ev->pos);   break;
        case EV_LOCKED:        play_at(audio, SND_DOOR_LOCKED, 0.7f, ear, ev->pos);  break;
        case EV_BUTTON_PRESSED: play_at(audio, SND_SWITCH, 0.7f, ear, ev->pos);      break;
        case EV_LIGHT_SHOT:    play_at(audio, SND_GLASS, 0.8f, ear, ev->pos);        break;
        case EV_TRIGGERED:     break; /* game.c shows its message */
        case EV_PLAYER_HURT:   play_at(audio, SND_PLAYER_PAIN, 0.7f, ear, ev->pos);  break;
        case EV_PLAYER_DIED:   play_at(audio, SND_PLAYER_DEATH, 0.8f, ear, ev->pos); break;
        case EV_ENEMY_ALERT:   play_at(audio, sounds_of(g, ev->entity)->alert, 0.7f, ear, ev->pos);  break;
        case EV_ENEMY_ATTACK:  play_at(audio, sounds_of(g, ev->entity)->attack, 0.7f, ear, ev->pos); break;
        case EV_ENEMY_PAIN:    play_at(audio, sounds_of(g, ev->entity)->pain, 0.6f, ear, ev->pos);   break;
        case EV_ENEMY_DIED:    play_at(audio, sounds_of(g, ev->entity)->death, 0.8f, ear, ev->pos);  break;
        case EV_LEVEL_EXIT:    play_at(audio, SND_DOOR_OPEN, 0.8f, ear, ev->pos);    break;
        }
    }
}

/* Fog the camera sees: its zone's (environment.h), faded in over
 * ZONE_FADE_TIME so walking into a foggy room never pops */
#define ZONE_FADE_TIME 0.6f /* seconds to get ~63% of the way */

typedef struct {
    Environment env;
    bool started;   /* false until the first frame: start at the target */
} ViewEnv;

static void fade_view_env(ViewEnv *v, const Scene *scene, Vec3 eye, float dt) {
    Environment target = environment_for_view(&scene->env, &scene->zones, eye);
    if (!v->started) {
        v->env = target;
        v->started = true;
        return;
    }
    v->env = environment_blend(&v->env, &target, 1.0f - expf(-dt / ZONE_FADE_TIME));
}

/* Color drains from the world as health drops below LOW_HEALTH */
#define LOW_HEALTH       35.0f
#define LOW_HEALTH_DRAIN 0.85f /* at 0 health: nearly grey */

static float low_health_drain(float health) {
    if (health <= 0 || health >= LOW_HEALTH) {
        return 0; /* dead: the red death tint takes over */
    }
    return (1.0f - health / LOW_HEALTH) * LOW_HEALTH_DRAIN;
}

/* --- Ambience: the listener's zone names a background loop; loops
 * crossfade over AMBIENCE_FADE so walking between rooms never cuts --- */

#define AMBIENCE_VOLUME 0.3f
#define AMBIENCE_FADE   1.5f /* seconds from silent to full */

typedef struct {
    const char *name;  /* as levels write it (environment.h) */
    SoundID sound;
} AmbienceSound;

static const AmbienceSound ambience_sounds[] = {
    {"drone", SND_AMBIENT},
    {"drip",  SND_AMBIENT_DRIP},
    {"hum",   SND_AMBIENT_HUM},
};

#define AMBIENCE_COUNT ((int)(sizeof(ambience_sounds) / sizeof(ambience_sounds[0])))

typedef struct {
    int voice[AMBIENCE_COUNT];    /* looping voice, -1 while silent */
    float volume[AMBIENCE_COUNT];
} Ambience;

/* All loops silent; the level's ambience fades in on the first frames */
static void ambience_init(Ambience *a) {
    for (int k = 0; k < AMBIENCE_COUNT; k++) {
        a->voice[k] = -1;
        a->volume[k] = 0;
    }
}

/* Fade the named loop up and every other one down ("none": all down) */
static void update_ambience(Ambience *a, AudioSystem *audio, const char *name, float dt) {
    float step = AMBIENCE_VOLUME * dt / AMBIENCE_FADE;
    for (int k = 0; k < AMBIENCE_COUNT; k++) {
        float target = strcmp(name, ambience_sounds[k].name) == 0 ? AMBIENCE_VOLUME : 0;
        float v = a->volume[k];
        a->volume[k] = v < target ? fminf(v + step, target) : fmaxf(v - step, target);

        if (a->volume[k] <= 0) {
            audio_stop(audio, a->voice[k]);
            a->voice[k] = -1;
            continue;
        }
        if (a->voice[k] < 0) {
            a->voice[k] = audio_play_loop(audio, ambience_sounds[k].sound, a->volume[k]);
            continue;
        }
        audio_set_volume(audio, a->voice[k], a->volume[k]);
    }
}

/* HUD colors per keycard, matching the keycard sprites */
static const Vec3 keycard_colors[KEYCARD_COUNT] = {
    [KEYCARD_RED]    = {0.82f, 0.16f, 0.12f},
    [KEYCARD_BLUE]   = {0.16f, 0.35f, 0.86f},
    [KEYCARD_YELLOW] = {0.90f, 0.78f, 0.16f},
};

/* One HUD icon per held keycard, in Keycard order */
static void fill_keycards(OverlayState *o, const Inventory *inv) {
    for (int k = 0; k < KEYCARD_COUNT && o->keycard_count < OVERLAY_KEYCARDS; k++) {
        if (inventory_has_keycard(inv, (Keycard)k)) {
            o->keycard_colors[o->keycard_count++] = keycard_colors[k];
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

    Light flashlight = player_flashlight(p, game_view(g));
    float light = render_light_at(&g->scene, p->view.pos, g->time, &flashlight);
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
    ViewEnv view_env = {0};
    Ambience ambience;
    ambience_init(&ambience);
    static ParticleSystem particles;
    particles_init(&particles, (uint32_t)SDL_GetTicks());

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
    } else {
        printf("Audio init failed (continuing without sound)\n");
    }

    FileWatch level_watch;
    filewatch_init(&level_watch, level_file);
    float since_poll = 0;

    bool running = true;
    bool show_debug = false;
    bool affine = false;  /* PS1 texture warp, F4 */
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
        if (ui.toggle_affine) {
            affine = !affine;
        }
        if (ui.toggle_debug) {
            show_debug = !show_debug;
        }

        hot_reload(&game, &level_watch, &since_poll, dt);
        game_update(&game, &cmd, dt);
        play_event_sounds(&audio, &game.events, &game);
        play_event_effects(&fx, &game.events, &game);
        screenfx_update(&fx, dt);
        spawn_event_particles(&particles, &game.events, &game);
        particles_update(&particles, dt);

        /* Render: scene lights + flashlight, shadow rays (multithreaded) */
        /* Shake moves only what we see, never where shots go */
        Pose view = game_view(&game);
        float shake_yaw, shake_pitch;
        screenfx_camera_offset(&fx, game.time, &shake_yaw, &shake_pitch);
        view.yaw += shake_yaw;
        view.pitch += shake_pitch;
        Camera cam = camera_from_pose(view, VIEW_FOV);
        Light flashlight = player_flashlight(&game.player, view);
        fade_view_env(&view_env, &game.scene, cam.position, dt);
        audio_set_reverb(&audio, view_env.env.reverb);
        update_ambience(&ambience, &audio, view_env.env.ambience, dt);
        RenderView render_view = {.carried = &flashlight, .env = &view_env.env,
                                  .affine = affine};
        render_scene_lit_mt(&fb, &cam, &game.scene, game.time, &pool, &render_view);
        particles_draw(&particles, &fb, &cam, &view_env.env);
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
        overlay.message = game_message(&game);
        overlay.desaturate = low_health_drain(game.player.health);
        /* Battery bar only while it isn't full: in use, or recharging */
        overlay.show_battery = game.player.battery < 1.0f;
        overlay.battery = game.player.battery;
        fill_keycards(&overlay, &game.player.inv);
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
