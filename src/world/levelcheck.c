#include "levelcheck.h"
#include "mapfile.h"
#include <math.h>

#define FEET_LIFT        0.05f  /* start probes just above the feet */
#define FLOOR_SEARCH     50.0f  /* max distance to probe up (ceiling) or down (floor) */
#define PROBE_DIRECTIONS 8      /* horizontal rays around a body */
#define PROBE_REACH      0.9f   /* fraction of the radius probed: touching walls is fine */
#define KNEE_FRACTION    0.2f   /* probe heights as fractions of body height */
#define CHEST_FRACTION   0.6f
#define TWO_PI           6.28318530718f
#define NO_FLOOR         (-1.0f)

typedef struct {
    const Scene *scene;
    FILE *out;
    int warnings;
} Checker;

/* Static geometry only: entities may overlap each other legitimately */
static HitRecord trace(const Checker *c, Vec3 from, Vec3 dir, float dist) {
    Ray ray = {from, dir};
    return bvh_intersect(&c->scene->static_bvh, ray, 0.0f, dist);
}

/* Level-wide problem with no position */
static void warn_level(Checker *c, const char *what) {
    fprintf(c->out, "check: %s\n", what);
    c->warnings++;
}

static void warn(Checker *c, const char *what, Vec3 at) {
    /* Map units: inverse of map_to_engine() */
    float k = MAP_UNITS_PER_METER;
    fprintf(c->out, "check: %s\n       at (%.2f, %.2f, %.2f) m, map (%.0f %.0f %.0f)\n",
            what, at.x, at.y, at.z, at.x * k, -at.z * k, at.y * k);
    c->warnings++;
}

/* Inside a closed solid: looking straight up, the first surface faces
 * away from us (we see its back), however far up it is */
static bool inside_solid(const Checker *c, Vec3 feet) {
    Vec3 up = vec3(0, 1, 0);
    HitRecord hit = trace(c, vec3_add(feet, vec3(0, FEET_LIFT, 0)), up, FLOOR_SEARCH);
    return hit.hit && vec3_dot(hit.normal, up) > 0;
}

/* A wall cuts into the body: any horizontal probe at knee or chest
 * height hits within the radius */
static bool overlaps_wall(const Checker *c, Vec3 feet, BodySize body) {
    float heights[] = {body.height * KNEE_FRACTION, body.height * CHEST_FRACTION};
    for (int h = 0; h < 2; h++) {
        for (int i = 0; i < PROBE_DIRECTIONS; i++) {
            float a = TWO_PI * (float)i / PROBE_DIRECTIONS;
            Vec3 dir = vec3(cosf(a), 0, sinf(a));
            Vec3 from = vec3_add(feet, vec3(0, heights[h], 0));
            if (trace(c, from, dir, body.radius * PROBE_REACH).hit) {
                return true;
            }
        }
    }
    return false;
}

/* Distance from the feet down to a floor (>= 0), or NO_FLOOR */
static float floor_gap(const Checker *c, Vec3 feet) {
    Vec3 from = vec3_add(feet, vec3(0, FEET_LIFT, 0));
    HitRecord hit = trace(c, from, vec3(0, -1, 0), FLOOR_SEARCH);
    return hit.hit ? fmaxf(hit.t - FEET_LIFT, 0.0f) : NO_FLOOR;
}

static void check_spawn(Checker *c, const LevelInfo *info, BodySize player) {
    if (!info->has_spawn) {
        warn(c, "no player spawn (info_player_start / spawn); starting at the origin",
             info->spawn_pos);
    }

    Vec3 feet = info->spawn_pos;
    if (inside_solid(c, feet)) {
        warn(c, "player spawn is inside solid geometry", feet);
    } else if (overlaps_wall(c, feet, player)) {
        warn(c, "player spawn overlaps a wall", feet);
    }
    if (floor_gap(c, feet) == NO_FLOOR) {
        warn(c, "no floor below the player spawn", feet);
    }
}

/* Enemies: anything with a damage hook. Their body comes from the sprite. */
static void check_enemy(Checker *c, const Entity *e, int index, const Sprite *sp) {
    const EntityDef *def = entity_def(e->type);
    BodySize body = {sp->width * 0.5f, sp->height};
    char what[128];

    if (inside_solid(c, e->position)) {
        snprintf(what, sizeof(what), "%s #%d is inside solid geometry", def->name, index);
        warn(c, what, e->position);
    } else if (overlaps_wall(c, e->position, body)) {
        snprintf(what, sizeof(what), "%s #%d overlaps a wall", def->name, index);
        warn(c, what, e->position);
    }

    /* Enemies fall onto whatever is below; only a bottomless drop is wrong */
    if (floor_gap(c, e->position) == NO_FLOOR) {
        snprintf(what, sizeof(what), "%s #%d has no floor below it", def->name, index);
        warn(c, what, e->position);
    }
}

static void check_entities(Checker *c) {
    const EntityList *list = &c->scene->entities;
    bool has_exit = false;

    for (int i = 0; i < list->count; i++) {
        const Entity *e = &list->items[i];
        const EntityDef *def = entity_def(e->type);
        has_exit |= def->touch_effect == TOUCH_EFFECT_EXIT;
        if (!def->damage || !def->sprite) {
            continue;
        }
        Sprite sp = {.frame_count = 1};
        def->sprite(e, &sp);
        check_enemy(c, e, i, &sp);
    }

    if (!has_exit) {
        warn_level(c, "no exit pad; the level can't be finished");
    }
}

static void check_budget(Checker *c) {
    const Scene *s = c->scene;
    int tris = s->static_bvh.tri_count;
    int lights = s->lights.count;
    int sprites = entity_count_sprites(&s->entities);

    fprintf(c->out, "budget: %d/%d triangles, %d/%d lights, %d/%d sprites\n",
            tris, BUDGET_TRIANGLES, lights, BUDGET_LIGHTS, sprites, BUDGET_SPRITES);

    if (tris > BUDGET_TRIANGLES || lights > BUDGET_LIGHTS || sprites > BUDGET_SPRITES) {
        warn_level(c, "over budget; expect frames slower than 60 fps");
    }
}

int level_check(const Scene *s, const LevelInfo *info, BodySize player, FILE *out) {
    Checker c = {.scene = s, .out = out};
    check_spawn(&c, info, player);
    check_entities(&c);
    check_budget(&c);
    return c.warnings;
}
