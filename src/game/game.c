#include "game.h"
#include "level.h"
#include <math.h>

#define DEG_TO_RAD ((float)M_PI / 180.0f)

GameLoadResult game_init(Game *g, const char *level_path) {
    *g = (Game){.mode = GAME_PLAYING};
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

void game_tick(Game *g, const PlayerCmd *cmd) {
    const float dt = GAME_TICK_DT;
    g->time += dt;

    /* World first, so the player collides with this tick's entity poses */
    entity_update(&g->scene.entities, dt, g->time);
    light_update(&g->scene.lights, g->time);
    scene_rebuild_dynamic(&g->scene);

    player_update(&g->player, cmd, dt, &g->scene, &g->events);
    weapon_update(&g->weapon, cmd, g->player.view, &g->scene, dt, &g->events);
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
