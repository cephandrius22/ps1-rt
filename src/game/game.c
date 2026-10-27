#include "game.h"
#include "level.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define DEG_TO_RAD       ((float)M_PI / 180.0f)
#define DEAD_EYE_HEIGHT  0.3f /* death cam: eye this far above the feet */
#define DEAD_EYE_SPEED   2.0f /* how fast the view sinks, units/second */

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

    return GAME_LOAD_OK;
}

void game_free(Game *g) {
    scene_free(&g->scene);
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

static void tick_playing(Game *g, const PlayerCmd *cmd, float dt) {
    player_update(&g->player, cmd, dt, &g->scene, &g->events);
    weapon_update(&g->weapon, cmd, game_view(g), &g->scene, dt,
                  &g->player.inv, &g->events);

    if (!player_alive(&g->player)) {
        set_mode(g, GAME_DEAD);
    }
}

/* Sink the view to the floor; respawn on attack/use once the delay passed */
static void tick_dead(Game *g, const PlayerCmd *cmd, float dt) {
    Player *p = &g->player;
    float floor_eye = p->foot_y + DEAD_EYE_HEIGHT;
    p->view.pos.y = fmaxf(p->view.pos.y - DEAD_EYE_SPEED * dt, floor_eye);

    bool wants_respawn = cmd_has(cmd, BTN_ATTACK) || cmd_has(cmd, BTN_USE);
    if (g->mode_time >= GAME_RESPAWN_DELAY && wants_respawn) {
        restart_level(g);
    }
}

void game_tick(Game *g, const PlayerCmd *cmd) {
    const float dt = GAME_TICK_DT;
    g->time += dt;
    g->mode_time += dt;

    /* World first, so the player collides with this tick's entity poses */
    entity_update(&g->scene.entities, dt, g->time);
    light_update(&g->scene.lights, g->time);
    scene_rebuild_dynamic(&g->scene);

    switch (g->mode) {
    case GAME_PLAYING:
        tick_playing(g, cmd, dt);
        break;
    case GAME_DEAD:
        tick_dead(g, cmd, dt);
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
