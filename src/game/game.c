#include "game.h"
#include "level.h"
#include "ai.h"
#include "levelcheck.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define DEG_TO_RAD       ((float)M_PI / 180.0f)
#define DEAD_EYE_HEIGHT  0.3f /* death cam: eye this far above the feet */
#define DEAD_EYE_SPEED   2.0f /* how fast the view sinks, units/second */

static BodySize player_body(void) {
    return (BodySize){PLAYER_RADIUS, PLAYER_HEIGHT};
}

static int count_enemies(const EntityList *list) {
    int n = 0;
    for (int i = 0; i < list->count; i++) {
        n += enemy_def(list->items[i].type) != NULL;
    }
    return n;
}

GameLoadResult game_init(Game *g, const char *level_path) {
    *g = (Game){.mode = GAME_PLAYING};
    snprintf(g->level_path, sizeof(g->level_path), "%s", level_path);
    scene_init(&g->scene);

    LevelInfo info;
    if (level_load(&g->scene, level_path, &info) != 0) {
        scene_free(&g->scene);
        return GAME_LOAD_FAILED;
    }

    /* Spawn point is at the feet; the player is positioned by its eye */
    Vec3 eye = vec3(info.spawn_pos.x,
                    info.spawn_pos.y + PLAYER_HEIGHT + PLAYER_EYE_OFFSET,
                    info.spawn_pos.z);
    g->player = player_create(eye);
    g->player.view.yaw = info.spawn_yaw * DEG_TO_RAD;
    g->weapon = weapon_create(WEAPON_PISTOL);
    g->kills_total = count_enemies(&g->scene.entities);

    level_check(&g->scene, &info, player_body(), stderr);
    return GAME_LOAD_OK;
}

void game_free(Game *g) {
    scene_free(&g->scene);
}

GameLoadResult game_reload(Game *g) {
    /* Load beside the running level so a broken save changes nothing */
    Scene fresh;
    scene_init(&fresh);
    LevelInfo info;
    if (level_load(&fresh, g->level_path, &info) != 0) {
        scene_free(&fresh);
        return GAME_LOAD_FAILED;
    }

    level_check(&fresh, &info, player_body(), stderr);
    scene_free(&g->scene);
    g->scene = fresh;
    g->kills = 0;
    g->kills_total = count_enemies(&g->scene.entities);
    g->mode = GAME_PLAYING;
    g->mode_time = 0;

    /* Same spot and view, fresh body: designers iterate, not survive */
    Player kept = g->player;
    g->player = player_create(kept.view.pos);
    g->player.view = kept.view;
    g->player.foot_y = kept.foot_y;
    g->player.on_ground = kept.on_ground;
    g->player.flashlight_on = kept.flashlight_on;
    return GAME_LOAD_OK;
}

static void set_mode(Game *g, GameMode mode) {
    g->mode = mode;
    g->mode_time = 0;
}

/* Doom-style respawn: reload the level from scratch */
static void restart_level(Game *g) {
    char path[GAME_LEVEL_PATH_MAX];
    snprintf(path, sizeof(path), "%s", g->level_path);

    /* game_update() is mid-loop: keep its timing and pending input */
    float tick_accum = g->tick_accum;
    PlayerCmd pending = g->pending;
    game_free(g);

    if (game_init(g, path) != GAME_LOAD_OK) {
        fprintf(stderr, "game: failed to reload %s\n", path);
        scene_init(&g->scene);
        set_mode(g, GAME_DEAD);
    }
    g->tick_accum = tick_accum;
    g->pending = pending;
}

Pose game_view(const Game *g) {
    Pose view = g->player.view;
    view.pitch += g->weapon.kick;
    return view;
}

int game_ammo(const Game *g) {
    AmmoType type = weapon_def(g->weapon.type)->ammo;
    return g->player.inv.ammo[type];
}

/* Turn this tick's bullet impacts into damage on whatever they hit */
static void resolve_impacts(Game *g, int first_event) {
    int end = g->events.count;
    for (int i = first_event; i < end; i++) {
        GameEvent hit = g->events.items[i];
        if (hit.type != EV_WEAPON_IMPACT || hit.entity == ENTITY_INDEX_NONE) {
            continue;
        }

        DamageResult res = entity_damage(&g->scene.entities, hit.entity, hit.amount);
        if (res == DAMAGE_IGNORED) {
            continue;
        }
        const Entity *e = &g->scene.entities.items[hit.entity];
        GameEventType type = res == DAMAGE_KILLED ? EV_ENEMY_DIED : EV_ENEMY_PAIN;
        g->kills += res == DAMAGE_KILLED;
        events_push(&g->events, (GameEvent){.type = type, .pos = e->position,
                                            .entity = hit.entity, .amount = hit.amount});
    }
}

static bool has_event_since(const EventQueue *q, int first, GameEventType type) {
    for (int i = first; i < q->count; i++) {
        if (q->items[i].type == type) {
            return true;
        }
    }
    return false;
}

/* Next owned weapon after `from` in direction step (±1), wrapping;
 * `from` itself if nothing else is owned */
static WeaponType cycle_owned(const Inventory *inv, WeaponType from, int step) {
    int t = from;
    for (int i = 0; i < WEAPON_TYPE_COUNT; i++) {
        t = (t + step + WEAPON_TYPE_COUNT) % WEAPON_TYPE_COUNT;
        if (inventory_has_weapon(inv, t)) {
            return (WeaponType)t;
        }
    }
    return from;
}

/* Number keys pick a slot (if owned); the wheel steps through owned ones */
static void select_weapon(Game *g, const PlayerCmd *cmd) {
    const Inventory *inv = &g->player.inv;
    int slot_type = cmd->weapon_slot - 1;
    if (cmd->weapon_slot > 0 && slot_type < WEAPON_TYPE_COUNT &&
        inventory_has_weapon(inv, slot_type)) {
        weapon_request(&g->weapon, (WeaponType)slot_type);
    }

    int steps = cmd->weapon_cycle;
    int dir = steps > 0 ? 1 : -1;
    WeaponType target = weapon_target(&g->weapon);
    for (int i = 0; i < steps * dir; i++) {
        target = cycle_owned(inv, target, dir);
    }
    weapon_request(&g->weapon, target);
}

/* Doom rule: picking up a weapon you didn't have switches to it */
static void switch_to_new_weapons(Game *g, int first_event) {
    for (int i = first_event; i < g->events.count; i++) {
        const GameEvent *ev = &g->events.items[i];
        if (ev->type != EV_WEAPON_PICKUP || ev->amount == 0) {
            continue;
        }
        const Entity *e = &g->scene.entities.items[ev->entity];
        weapon_request(&g->weapon, weapon_type_by_name(e->pickup.item));
    }
}

#define MUZZLE_FORWARD 0.5f /* flash this far in front of the eye, meters */

/* Light the room for a moment when a shot leaves the barrel. No shadows:
 * a shadowed light costs a ray per pixel, too much for a 0.06 s flash. */
static void spawn_muzzle_flash(Game *g) {
    const WeaponDef *def = weapon_def(g->weapon.type);
    if (def->flash_intensity <= 0) {
        return;
    }

    Pose view = game_view(g);
    Vec3 ahead = pose_to_world_dir(view, vec3(0, 0, -1));
    PointLight flash = {
        .position = vec3_add(view.pos, vec3_mul(ahead, MUZZLE_FORWARD)),
        .color = def->flash_color,
        .intensity = def->flash_intensity,
        .radius = def->flash_radius,
        .cast_shadows = false,
    };
    light_add_temporary(&g->scene.lights, flash, def->flash_time);
}

static void tick_playing(Game *g, const PlayerCmd *cmd, float dt) {
    ai_update(&g->scene, &g->player, dt, &g->events);

    int first_event = g->events.count;
    player_update(&g->player, cmd, dt, &g->scene, &g->events);
    if (has_event_since(&g->events, first_event, EV_LEVEL_EXIT)) {
        set_mode(g, GAME_LEVEL_END);
        return;
    }
    switch_to_new_weapons(g, first_event);
    select_weapon(g, cmd);

    first_event = g->events.count;
    weapon_update(&g->weapon, cmd, game_view(g), &g->scene, dt,
                  &g->player.inv, &g->events);
    if (g->weapon.fired_this_tick) {
        spawn_muzzle_flash(g);
    }
    resolve_impacts(g, first_event);

    if (!player_alive(&g->player)) {
        set_mode(g, GAME_DEAD);
    }
}

/* After the delay, attack/use reloads the level (respawn or replay) */
static void continue_on_input(Game *g, const PlayerCmd *cmd) {
    bool pressed = cmd_has(cmd, BTN_ATTACK) || cmd_has(cmd, BTN_USE);
    if (g->mode_time >= GAME_CONTINUE_DELAY && pressed) {
        restart_level(g);
    }
}

/* Enemies keep prowling; the view sinks to the floor */
static void tick_dead(Game *g, const PlayerCmd *cmd, float dt) {
    ai_update(&g->scene, &g->player, dt, &g->events);

    Player *p = &g->player;
    float floor_eye = p->foot_y + DEAD_EYE_HEIGHT;
    p->view.pos.y = fmaxf(p->view.pos.y - DEAD_EYE_SPEED * dt, floor_eye);
    continue_on_input(g, cmd);
}

void game_tick(Game *g, const PlayerCmd *cmd) {
    const float dt = GAME_TICK_DT;
    g->time += dt;
    g->mode_time += dt;

    /* World first, so the player collides with this tick's entity poses */
    entity_update(&g->scene.entities, dt, g->time);
    light_update(&g->scene.lights, g->time, dt);
    scene_rebuild_dynamic(&g->scene);

    switch (g->mode) {
    case GAME_PLAYING:
        tick_playing(g, cmd, dt);
        break;
    case GAME_DEAD:
        tick_dead(g, cmd, dt);
        break;
    case GAME_LEVEL_END:
        continue_on_input(g, cmd);
        break;
    }
}

int game_update(Game *g, const PlayerCmd *cmd, float frame_dt) {
    events_clear(&g->events);
    cmd_accumulate(&g->pending, cmd);
    g->tick_accum += frame_dt;

    int ticks = 0;
    while (g->tick_accum >= GAME_TICK_DT && ticks < GAME_MAX_TICKS_PER_FRAME) {
        game_tick(g, &g->pending);
        cmd_consume(&g->pending);
        g->tick_accum -= GAME_TICK_DT;
        ticks++;
    }

    /* Too far behind (debugger, hitch): skip the backlog, keep remainder */
    if (g->tick_accum >= GAME_TICK_DT) {
        g->tick_accum = fmodf(g->tick_accum, GAME_TICK_DT);
    }
    return ticks;
}
