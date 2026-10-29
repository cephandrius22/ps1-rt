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
 *                                                        ──> display_present()
 *
 * The level file is watched: saving it in an editor reloads it in place.
 *
 * Holds no game rules: it only translates between game/, render/ and
 * platform/ (event → sound, game mode → overlay mode). SDL itself stays
 * in platform/ (display, input, audio, clock).
 */
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
#include "display.h"
#include "clock.h"
#include "settings.h"
#include "files.h"
#include "save.h"
#include "menu.h"
#include "editor_export.h"
#include "level_convert.h"
#include "screenfx.h"
#include "particles.h"
#include "levelcheck.h"
#include "mapfile.h"

#define WINDOW_SCALE 3
#define WINDOW_TITLE "PS1 Raytracer"
#define DEG_TO_RAD   ((float)MATH_PI / 180.0f)
#define DEFAULT_START "levels/episode1.txt" /* an episode file or a level */
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
#define BLAST_SHAKE       0.08f  /* camera jolt at the center of an explosion */
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
#define BLOOD_COUNT   16
#define BLOOD_SPEED   2.0f
#define GIB_COUNT     18
#define GIB_SPEED     3.0f
#define GIB_BLOOD     28
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
            play_at(audio, weapon_fire_sounds[g->weapon.type], 1.0f, ear, ev->pos);
            break;
        case EV_EXPLOSION:     play_at(audio, burst_sound(g, ev->entity), 0.9f, ear, ev->pos); break;
        case EV_WEAPON_IMPACT: /* ricochet off walls, a thud into bodies */
            play_at(audio, ev->entity == ENTITY_INDEX_NONE ? SND_WEAPON_IMPACT : SND_FLESH_HIT,
                    0.6f, ear, ev->pos);
            break;
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
        case EV_SECRET_FOUND:  play_at(audio, SND_SECRET, 0.8f, ear, ev->pos);       break;
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

/* F5 / F9: save to and load from the quicksave, with a notice either way */
static void quick_save(Game *game) {
    bool saved = files_make_dir(SAVE_DIR) && game_save(game, SAVE_QUICK);
    game_notify(game, saved ? "Game saved" : "Can't save now");
}

static void quick_load(Game *game) {
    if (game_load(game, SAVE_QUICK) != GAME_LOAD_OK) {
        game_notify(game, "No quicksave to load");
    }
}

/* --- Menus: main.c carries out what the menu asks for --- */

/* The frame's UiCmd as the menu sees it (pointer in framebuffer pixels) */
static MenuInput menu_input(const UiCmd *ui, const Display *display) {
    MenuInput in = {.up = ui->up, .down = ui->down, .left = ui->left, .right = ui->right,
                    .select = ui->select, .back = ui->back, .click = ui->click,
                    .pointer_moved = ui->pointer_moved};
    display_to_frame(display, ui->pointer_x, ui->pointer_y, &in.pointer_x, &in.pointer_y);
    return in;
}

#define SLOT_EMPTY "EMPTY"
#define SECONDS_PER_MINUTE 60

/* "PUMPING STATION 3:12" per slot, or EMPTY */
static void refresh_slot_labels(Menu *menu) {
    for (int slot = 1; slot <= MENU_SLOTS; slot++) {
        char path[SAVE_PATH_MAX];
        SaveInfo info;
        save_slot_path(slot, path, sizeof(path));
        char *label = menu->slot_labels[slot - 1];
        if (!save_peek(path, &info)) {
            snprintf(label, OVERLAY_MENU_TEXT, SLOT_EMPTY);
            continue;
        }
        int t = (int)info.map_time;
        snprintf(label, OVERLAY_MENU_TEXT, "%.24s %d:%02d", info.map_title,
                 t / SECONDS_PER_MINUTE, t % SECONDS_PER_MINUTE);
    }
}

typedef struct {
    Game *game;
    const Episode *episode;
    Settings *settings;
    AudioSystem *audio;
    bool *running;
} MenuTargets;

static void apply_settings(const Settings *s, AudioSystem *audio) {
    input_set_sensitivity(s->sensitivity);
    audio_set_master(audio, s->volume);
}

static void do_menu_action(MenuAction act, Menu *menu, MenuTargets t) {
    char path[SAVE_PATH_MAX];
    switch (act.event) {
    case MENU_NONE:
    case MENU_RESUME:
        return;
    case MENU_NEW_GAME:
        game_free(t.game);
        if (game_start(t.game, t.episode) != GAME_LOAD_OK) {
            fprintf(stderr, "New game: failed to load the first map\n");
        }
        return;
    case MENU_SAVE_SLOT:
        save_slot_path(act.slot, path, sizeof(path));
        if (files_make_dir(SAVE_DIR) && game_save(t.game, path)) {
            game_notify(t.game, "Game saved");
            menu->screen = MENU_CLOSED;
        }
        refresh_slot_labels(menu);
        return;
    case MENU_LOAD_SLOT:
        save_slot_path(act.slot, path, sizeof(path));
        if (game_load(t.game, path) == GAME_LOAD_OK) {
            menu->screen = MENU_CLOSED;
        }
        return;
    case MENU_QUIT:
        *t.running = false;
        return;
    case MENU_SETTINGS_CHANGED:
        *t.settings = menu->settings;
        apply_settings(t.settings, t.audio);
        return;
    case MENU_SETTINGS_DONE:
        settings_save(t.settings, SETTINGS_FILE);
        return;
    }
}

/* HUD colors per keycard, matching the keycard sprites */
static const Vec3 keycard_colors[KEYCARD_COUNT] = {
    [KEYCARD_RED]    = {0.82f, 0.16f, 0.12f},
    [KEYCARD_BLUE]   = {0.16f, 0.35f, 0.86f},
    [KEYCARD_YELLOW] = {0.90f, 0.78f, 0.16f},
};

/* Episode title, and the level-end tally: this map, the next, stats */
static void fill_intermission(OverlayState *o, const Game *g) {
    const Episode *ep = &g->episode;
    o->title = ep->title;
    o->map_title = ep->maps[g->map_index].title;
    o->next_title = g->map_index + 1 < ep->count ? ep->maps[g->map_index + 1].title : NULL;
    o->secrets = g->secrets;
    o->secrets_total = g->secrets_total;
    o->time_seconds = (int)g->map_time;
}

/* One HUD icon per held keycard, in Keycard order */
static void fill_keycards(OverlayState *o, const Inventory *inv) {
    for (int k = 0; k < KEYCARD_COUNT && o->keycard_count < OVERLAY_KEYCARDS; k++) {
        if (inventory_has_keycard(inv, (Keycard)k)) {
            o->keycard_colors[o->keycard_count++] = keycard_colors[k];
        }
    }
}

/* Every RELOAD_POLL_SEC, reload the level if its file was saved. A new
 * map (episode progress) moves the watch to its file. */
static void hot_reload(Game *game, FileWatch *watch, float *since_poll, float dt) {
    if (strcmp(watch->path, game->level_path) != 0) {
        filewatch_init(watch, game->level_path);
    }
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
#define VIEW_FIRE_SHOW    0.09f  /* seconds the firing frame stays up */
#define RECOIL_PX_PER_RAD 190.0f /* how far the gun jumps per radian of kick */
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
    case GAME_EPISODE_END: return OVERLAY_EPISODE_END;
    }
    return OVERLAY_PLAYING;
}

int main(int argc, char *argv[]) {
    /* Tool mode: write TrenchBroom textures + FGD, no window */
    if (argc > 2 && strcmp(argv[1], "--export-editor") == 0) {
        return editor_export(argv[2]) == 0 ? 0 : 1;
    }
    /* Tool mode: .level → TrenchBroom .map */
    if (argc > 3 && strcmp(argv[1], "--convert") == 0) {
        return level_convert(argv[2], argv[3]) == 0 ? 0 : 1;
    }

    Display display;
    if (!display_open(&display, WINDOW_TITLE, SCREEN_W, SCREEN_H, WINDOW_SCALE)) {
        return 1;
    }

    Framebuffer fb;
    Overlay hud;
    overlay_init(&hud);
    ViewAnim view_anim = {0};
    ScreenFx fx = {0};
    ViewEnv view_env = {0};
    Ambience ambience;
    ambience_init(&ambience);
    static ParticleSystem particles;
    particles_init(&particles, clock_seed());

    /* Game owns the level, player and weapon (static: Scene is large) */
    static Game game;
    const char *start = (argc > 1) ? argv[1] : DEFAULT_START;
    Episode episode;
    bool loaded = episode_is_file(start) ? episode_load(&episode, start) : true;
    if (!episode_is_file(start)) {
        episode = episode_single(start);
    }
    if (!loaded || game_start(&game, &episode) != GAME_LOAD_OK) {
        fprintf(stderr, "Failed to load: %s\n", start);
        display_close(&display);
        return 1;
    }

    input_init();

    /* Player preferences from rt.cfg (defaults on a first run) */
    Settings settings;
    settings_load(&settings, SETTINGS_FILE);
    input_set_sensitivity(settings.sensitivity);

    /* Create thread pool for parallel rendering */
    int num_threads = threadpool_cpu_count();
    ThreadPool pool;
    threadpool_create(&pool, num_threads);
    printf("Render threads: %d\n", num_threads);

    /* Initialize audio */
    AudioSystem audio;
    if (audio_init(&audio) == 0) {
        printf("Audio initialized\n");
        audio_set_master(&audio, settings.volume);
    } else {
        printf("Audio init failed (continuing without sound)\n");
    }

    FileWatch level_watch;
    filewatch_init(&level_watch, game.level_path);
    float since_poll = 0;

    /* The title screen opens over the first map, which waits paused */
    Menu menu;
    menu_init(&menu, settings, game.episode.title[0] ? game.episode.title : WINDOW_TITLE);
    menu_open(&menu, MENU_TITLE);
    MenuScreen last_screen = MENU_CLOSED;
    MenuTargets targets = {&game, &episode, &settings, &audio, NULL};

    bool running = true;
    targets.running = &running;
    bool show_debug = false;
    bool affine = false;  /* PS1 texture warp, F4 */
    float work_ms = 0;   /* smoothed */
    float fps = 0;       /* smoothed */
    double last_time = clock_seconds();

    while (running) {
        double frame_start = clock_seconds();
        float dt = (float)(frame_start - last_time);
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

        /* Esc while playing pauses; inside menus it goes back. The
         * frame a menu closes on, its click or Enter stays out of play. */
        bool was_paused = menu_is_open(&menu);
        if (!menu_is_open(&menu) && ui.back) {
            menu.can_save = game.mode == GAME_PLAYING;
            menu_open(&menu, MENU_PAUSE);
        } else if (menu_is_open(&menu)) {
            MenuInput in = menu_input(&ui, &display);
            do_menu_action(menu_update(&menu, &in), &menu, targets);
        }
        if (menu.screen != last_screen) {
            input_set_mouse(menu_is_open(&menu) ? MOUSE_FREE : MOUSE_CAPTURED);
            if (menu.screen == MENU_SAVE || menu.screen == MENU_LOAD) {
                refresh_slot_labels(&menu);
            }
            last_screen = menu.screen;
        }

        /* The game runs only with the menu closed; its events are this
         * frame's only then, so sounds and effects follow it */
        if (!menu_is_open(&menu) && !was_paused) {
            if (ui.quick_save) {
                quick_save(&game);
            }
            if (ui.quick_load) {
                quick_load(&game);
            }
            hot_reload(&game, &level_watch, &since_poll, dt);
            game_update(&game, &cmd, dt);
            play_event_sounds(&audio, &game.events, &game);
            play_event_effects(&fx, &game.events, &game);
            screenfx_update(&fx, dt);
            spawn_event_particles(&particles, &game.events, &game);
            particles_update(&particles, dt);
        }

        /* Render: scene lights + flashlight, shadow rays (multithreaded) */
        /* Shake moves only what we see, never where shots go */
        Pose view = game_view(&game);
        float shake_yaw, shake_pitch;
        screenfx_camera_offset(&fx, game.time, &shake_yaw, &shake_pitch);
        view.yaw += shake_yaw;
        view.pitch += shake_pitch;
        Camera cam = camera_from_pose(view, settings.fov * DEG_TO_RAD);
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
        fill_intermission(&overlay, &game);
        overlay.menu = menu_view(&menu);
        fill_viewmodel(&overlay.viewmodel, &view_anim, &game, dt);
        if (show_debug) {
            fill_debug(&overlay, &game, work_ms, fps);
        }
        overlay_draw(&hud, &fb, &overlay);

        /* Work time: everything this frame except waiting for vsync */
        float frame_work = (float)(clock_seconds() - frame_start) * 1000.0f;
        work_ms += (frame_work - work_ms) * MS_SMOOTHING;
        fps += ((dt > 0 ? 1.0f / dt : 0) - fps) * MS_SMOOTHING;

        display_present(&display, fb.pixels);

        float frame_ms = (float)(clock_seconds() - frame_start) * 1000.0f;
        printf("\rFrame: %.1f ms (%.0f fps)  Entities: %d  Lights: %d",
               frame_ms, 1000.0f / frame_ms, game.scene.entities.count, game.scene.lights.count);
        fflush(stdout);
    }

    printf("\n");
    audio_shutdown(&audio);
    threadpool_destroy(&pool);
    overlay_free(&hud);
    game_free(&game);
    display_close(&display);
    return 0;
}
