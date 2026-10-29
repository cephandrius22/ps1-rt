#include "projectile.h"
#include "damage.h"
#include "proctex.h" /* ProjectileFrame */
#include <math.h>

#define TRACE_EPS        0.001f
#define OWNER_SKIP_MAX   4      /* times a trace may pass through its shooter */
#define BURST_PULLBACK   0.1f   /* explode slightly off the surface hit */
#define BODY_CENTER      0.8f   /* splash aims at this height above an enemy's feet */
#define CHEST_BELOW_EYE  0.5f   /* splash aims here on the player */
#define GLOW_TTL_TICKS   1.5f   /* in-flight light lives this many ticks, refreshed each tick */

/* trace_skip() target that matches no hit (world hits report
 * ENTITY_INDEX_NONE, so that can't mean "skip nothing") */
#define SKIP_NOTHING     PROJECTILE_OWNER_PLAYER

static const ProjectileDef projectile_defs[PROJECTILE_KIND_COUNT] = {
    [PROJECTILE_ROCKET] = {
        .speed = 18.0f, .damage = 90.0f,
        .splash_radius = 2.8f, .splash_damage = 110.0f,
        .hit_radius = 0.15f, .life = 6.0f,
        .frame = PROJ_FRAME_ROCKET, .size = 0.35f,
        .glow_color = {1.0f, 0.6f, 0.25f}, .glow_intensity = 1.4f, .glow_radius = 4.0f,
        .burst_color = {1.0f, 0.55f, 0.2f}, .burst_intensity = 5.5f,
        .burst_radius = 12.0f, .burst_time = 0.4f,
    },
    [PROJECTILE_SPIT] = {
        .speed = 8.0f, .damage = 14.0f,
        .splash_radius = 0.0f, .splash_damage = 0.0f,
        .hit_radius = 0.2f, .life = 4.0f,
        .frame = PROJ_FRAME_SPIT, .size = 0.4f,
        .glow_color = {0.4f, 1.0f, 0.2f}, .glow_intensity = 1.0f, .glow_radius = 3.5f,
        .burst_color = {0.4f, 1.0f, 0.2f}, .burst_intensity = 1.5f,
        .burst_radius = 4.0f, .burst_time = 0.2f,
    },
};

const ProjectileDef *projectile_def(ProjectileKind kind) {
    if (kind <= PROJECTILE_NONE || kind >= PROJECTILE_KIND_COUNT) {
        return &projectile_defs[PROJECTILE_ROCKET];
    }
    return &projectile_defs[kind];
}

int projectile_fire(Scene *s, ProjectileKind kind, Vec3 from, Vec3 dir, int owner) {
    const ProjectileDef *def = projectile_def(kind);
    Entity e = {
        .type = ENTITY_PROJECTILE,
        .position = from,
        .active = true,
    };
    e.projectile.kind = kind;
    e.projectile.frame = def->frame;
    e.projectile.size = def->size;
    e.projectile.velocity = vec3_mul(vec3_normalize(dir), def->speed);
    e.projectile.owner = owner;
    e.projectile.life = def->life;
    return scene_spawn(s, e);
}

/* --- Collision --- */

/* Closest solid hit along the ray within dist, passing through the
 * shooter itself (a ghoul's spit starts inside the ghoul's sprite) */
static HitRecord trace_skip(const Scene *s, Ray ray, float dist, int skip) {
    float t_min = TRACE_EPS;
    for (int i = 0; i < OWNER_SKIP_MAX; i++) {
        HitRecord hit = scene_trace(s, ray, t_min, dist, TRACE_SOLID);
        if (!hit.hit || entity_hit(&hit) != skip) {
            return hit;
        }
        t_min = hit.t + TRACE_EPS;
    }
    return (HitRecord){.hit = false};
}

/* Distance along the step where it passes through the player's body
 * cylinder (fattened by the projectile's radius), or -1. Uses the closest
 * approach to the body's vertical axis in the XZ plane:
 *
 *   top view:   pos ──────●────────> pos + step
 *                         ╎ closest
 *                        (P) player axis, radius R + r
 */
static float hits_player(const Player *p, Vec3 pos, Vec3 step, float radius) {
    float dx = step.x;
    float dz = step.z;
    float fx = pos.x - p->view.pos.x;
    float fz = pos.z - p->view.pos.z;
    float a = dx * dx + dz * dz;
    float t = a > 1e-9f ? -(fx * dx + fz * dz) / a : 0.0f;
    t = fmaxf(0.0f, fminf(1.0f, t));

    float cx = fx + dx * t;
    float cz = fz + dz * t;
    float reach = PLAYER_RADIUS + radius;
    if (cx * cx + cz * cz > reach * reach) {
        return -1.0f;
    }

    float y = pos.y + step.y * t;
    if (y < p->foot_y - radius || y > p->foot_y + PLAYER_HEIGHT + radius) {
        return -1.0f;
    }
    return t * vec3_length(step);
}

/* Nothing solid between from and to, except the target itself */
static bool clear_line(const Scene *s, Vec3 from, Vec3 to, int target) {
    Vec3 d = vec3_sub(to, from);
    float dist = vec3_length(d);
    if (dist < TRACE_EPS) {
        return true;
    }
    Ray ray = {from, vec3_mul(d, 1.0f / dist)};
    HitRecord hit = trace_skip(s, ray, dist, target);
    return !hit.hit;
}

/* --- Explosion --- */

/* Splash: everything damageable within the radius with a clear line from
 * the blast gets damage falling off linearly with distance */
static void splash(Scene *s, Player *p, const ProjectileDef *def, Vec3 at, EventQueue *ev) {
    float r = def->splash_radius;
    if (r <= 0) {
        return;
    }

    EntityList *list = &s->entities;
    for (int i = 0; i < list->count; i++) {
        const Entity *e = &list->items[i];
        if (!e->active || !entity_def(e->type)->damage) {
            continue;
        }
        Vec3 center = vec3_add(e->position, vec3(0, BODY_CENTER, 0));
        float dist = vec3_length(vec3_sub(center, at));
        if (dist < r && clear_line(s, at, center, i)) {
            damage_entity(s, i, def->splash_damage * (1.0f - dist / r), at, ev);
        }
    }

    Vec3 chest = vec3_sub(p->view.pos, vec3(0, CHEST_BELOW_EYE, 0));
    float dist = vec3_length(vec3_sub(chest, at));
    if (dist < r && clear_line(s, at, chest, SKIP_NOTHING)) {
        player_damage(p, def->splash_damage * (1.0f - dist / r), ev);
    }
}

typedef enum {
    TARGET_NONE,
    TARGET_ENTITY,
    TARGET_PLAYER,
} TargetKind;

static void explode(Scene *s, Player *p, int index, Vec3 at, TargetKind target,
                    int target_entity, EventQueue *ev) {
    Entity *e = &s->entities.items[index];
    const ProjectileDef *def = projectile_def(e->projectile.kind);
    e->active = false; /* before damage: nothing should see it mid-blast */

    if (target == TARGET_ENTITY) {
        /* Shoved along the shot: "from" a point back along its flight */
        Vec3 behind = vec3_sub(at, e->projectile.velocity);
        damage_entity(s, target_entity, def->damage, behind, ev);
    } else if (target == TARGET_PLAYER) {
        player_damage(p, def->damage, ev);
    }
    splash(s, p, def, at, ev);

    Light burst = {
        .position = at, .color = def->burst_color,
        .intensity = def->burst_intensity, .radius = def->burst_radius,
    };
    light_add_temporary(&s->lights, burst, def->burst_time);
    events_push(ev, (GameEvent){.type = EV_EXPLOSION, .pos = at,
                                .entity = index, .amount = def->splash_radius});
}

/* --- Flight --- */

static void fly(Scene *s, Player *p, int index, float dt, EventQueue *ev) {
    Entity *e = &s->entities.items[index];
    const ProjectileDef *def = projectile_def(e->projectile.kind);
    Vec3 pos = e->position;
    Vec3 step = vec3_mul(e->projectile.velocity, dt);
    float len = vec3_length(step);
    Vec3 dir = vec3_mul(step, 1.0f / len);

    /* Reach one body radius past the step: what the projectile touches
     * counts, and it can't stop exactly on a wall and tunnel next tick */
    HitRecord hit = trace_skip(s, (Ray){pos, dir}, len + def->hit_radius, e->projectile.owner);
    float player_t = e->projectile.owner == PROJECTILE_OWNER_PLAYER
                   ? -1.0f : hits_player(p, pos, step, def->hit_radius);

    /* Nearest of: the player, something in the scene */
    if (player_t >= 0 && (!hit.hit || player_t <= hit.t)) {
        explode(s, p, index, vec3_add(pos, vec3_mul(dir, player_t)), TARGET_PLAYER, 0, ev);
        return;
    }
    if (hit.hit) {
        Vec3 at = vec3_add(pos, vec3_mul(dir, fmaxf(hit.t - BURST_PULLBACK, 0.0f)));
        int target = entity_hit(&hit);
        explode(s, p, index, at, target == ENTITY_INDEX_NONE ? TARGET_NONE : TARGET_ENTITY,
                target, ev);
        return;
    }

    e->position = vec3_add(pos, step);
    e->projectile.life -= dt;
    if (e->projectile.life <= 0) {
        explode(s, p, index, e->position, TARGET_NONE, 0, ev);
        return;
    }

    if (def->glow_intensity > 0) {
        Light glow = {
            .position = e->position, .color = def->glow_color,
            .intensity = def->glow_intensity, .radius = def->glow_radius,
        };
        light_add_temporary(&s->lights, glow, dt * GLOW_TTL_TICKS);
    }
}

void projectile_update(Scene *s, Player *p, float dt, EventQueue *ev) {
    EntityList *list = &s->entities;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].active && list->items[i].type == ENTITY_PROJECTILE) {
            fly(s, p, i, dt, ev);
        }
    }
}
