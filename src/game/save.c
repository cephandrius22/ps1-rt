#include "save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_MAGIC   "RTSAVE"
#define SAVE_VERSION 1u
#define MAGIC_LEN    8

/* Sizes of everything written raw: a different build won't match */
typedef enum {
    SIZE_ENTITY, SIZE_LIGHT, SIZE_PLAYER, SIZE_WEAPON, SIZE_LOADOUT, SIZE_EPISODE,
    SIZE_COUNT
} SavedSize;

typedef struct {
    char magic[MAGIC_LEN];
    uint32_t version;
    uint32_t sizes[SIZE_COUNT];
} SaveHeader;

/* The game fields a save restores (not the scene) */
typedef struct {
    Episode episode;
    int map_index;
    Loadout entry;
    Player player;
    Weapon weapon;
    int kills, kills_total, secrets, secrets_total;
    float time, map_time;
} SavedGame;

static SaveHeader expected_header(void) {
    SaveHeader h = {.version = SAVE_VERSION,
                    .sizes = {sizeof(Entity), sizeof(Light), sizeof(Player), sizeof(Weapon),
                              sizeof(Loadout), sizeof(Episode)}};
    memcpy(h.magic, SAVE_MAGIC, sizeof(SAVE_MAGIC));
    return h;
}

void save_slot_path(int slot, char *out, int size) {
    snprintf(out, (size_t)size, SAVE_DIR "/slot%d.sav", slot);
}

/* --- Writing --- */

/* fwrite that remembers failure, so the caller checks once at the end */
static void put(FILE *f, const void *data, size_t size, bool *ok) {
    *ok = *ok && fwrite(data, size, 1, f) == 1;
}

bool game_save(const Game *g, const char *path) {
    if (g->mode != GAME_PLAYING) {
        return false;
    }
    FILE *f = fopen(path, "wb");
    if (!f) {
        return false;
    }

    SaveHeader header = expected_header();
    SaveInfo info = {.map_index = g->map_index, .map_time = g->map_time};
    snprintf(info.episode_title, sizeof(info.episode_title), "%s", g->episode.title);
    snprintf(info.map_title, sizeof(info.map_title), "%s", g->episode.maps[g->map_index].title);
    SavedGame saved = {
        .episode = g->episode, .map_index = g->map_index, .entry = g->entry,
        .player = g->player, .weapon = g->weapon,
        .kills = g->kills, .kills_total = g->kills_total,
        .secrets = g->secrets, .secrets_total = g->secrets_total,
        .time = g->time, .map_time = g->map_time,
    };
    const EntityList *ents = &g->scene.entities;
    const LightList *lights = &g->scene.lights;

    bool ok = true;
    put(f, &header, sizeof(header), &ok);
    put(f, &info, sizeof(info), &ok);
    put(f, &saved, sizeof(saved), &ok);
    put(f, &ents->count, sizeof(ents->count), &ok);
    if (ents->count > 0) {
        put(f, ents->items, sizeof(Entity) * (size_t)ents->count, &ok);
    }
    put(f, &lights->count, sizeof(lights->count), &ok);
    if (lights->count > 0) {
        put(f, lights->items, sizeof(Light) * (size_t)lights->count, &ok);
    }
    ok = fclose(f) == 0 && ok;
    return ok;
}

/* --- Reading --- */

static bool get(FILE *f, void *data, size_t size) {
    return fread(data, size, 1, f) == 1;
}

/* Header and info; f is left at the game block */
static bool read_front(FILE *f, SaveInfo *info) {
    SaveHeader header, want = expected_header();
    return get(f, &header, sizeof(header)) && memcmp(&header, &want, sizeof(header)) == 0 &&
           get(f, info, sizeof(*info));
}

bool save_peek(const char *path, SaveInfo *info) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    bool ok = read_front(f, info);
    fclose(f);
    return ok;
}

/* A counted array of `size`-byte records, malloc'd; NULL count 0 is fine */
static bool get_array(FILE *f, size_t size, void **items, int *count) {
    *items = NULL;
    if (!get(f, count, sizeof(*count)) || *count < 0) {
        return false;
    }
    if (*count == 0) {
        return true;
    }
    *items = malloc(size * (size_t)*count);
    return *items && get(f, *items, size * (size_t)*count);
}

/* The freshly loaded map's entities must be the saved map's: same
 * count (or more saved, for rockets and spit spawned in play), same types */
static bool same_map(const EntityList *fresh, const Entity *saved, int saved_count) {
    if (saved_count < fresh->count) {
        return false;
    }
    for (int i = 0; i < fresh->count; i++) {
        if (fresh->items[i].type != saved[i].type) {
            return false;
        }
    }
    return true;
}

/* Put the saved entities and lights in place of the fresh map's */
static void restore_scene(Scene *s, const Entity *ents, int ent_count, const Light *lights,
                          int light_count) {
    s->entities.count = 0;
    for (int i = 0; i < ent_count; i++) {
        Entity e = ents[i];
        if (e.type == ENTITY_DOOR) {
            e.mover.nav_closed = false; /* fresh nav grid: logic re-marks it */
        }
        entity_add(&s->entities, e);
    }
    s->lights.count = 0;
    for (int i = 0; i < light_count; i++) {
        light_add(&s->lights, lights[i]);
    }
}

static void restore_game(Game *g, const SavedGame *saved) {
    g->player = saved->player;
    g->weapon = saved->weapon;
    g->weapon.last_hit = (HitRecord){0}; /* pointed into the old scene */
    g->kills = saved->kills;
    g->kills_total = saved->kills_total;
    g->secrets = saved->secrets;
    g->secrets_total = saved->secrets_total;
    g->time = saved->time;
    g->map_time = saved->map_time;
}

GameLoadResult game_load(Game *g, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return GAME_LOAD_FAILED;
    }
    SaveInfo info;
    SavedGame *saved = malloc(sizeof(SavedGame));
    Entity *ents = NULL;
    Light *lights = NULL;
    int ent_count = 0, light_count = 0;
    bool ok = saved && read_front(f, &info) && get(f, saved, sizeof(*saved)) &&
              get_array(f, sizeof(Entity), (void **)&ents, &ent_count) &&
              get_array(f, sizeof(Light), (void **)&lights, &light_count) &&
              saved->map_index >= 0 && saved->map_index < saved->episode.count;
    fclose(f);

    GameLoadResult result = GAME_LOAD_FAILED;
    if (ok) {
        g->episode = saved->episode;
        result = game_enter_map(g, saved->map_index, saved->entry);
    }
    if (result == GAME_LOAD_OK && same_map(&g->scene.entities, ents, ent_count)) {
        restore_scene(&g->scene, ents, ent_count, lights, light_count);
        restore_game(g, saved);
        game_notify(g, "Game loaded");
    } else if (result == GAME_LOAD_OK) {
        game_notify(g, "The map changed since this save; starting it fresh");
    }

    free(saved);
    free(ents);
    free(lights);
    return result;
}
