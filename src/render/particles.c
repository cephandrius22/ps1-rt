#include "particles.h"
#include "ps1_effects.h"

#define GRAVITY          9.8f
#define SPLAT_MIN_PX     1
#define SPLAT_MAX_PX     4
#define LIFE_JITTER      0.4f   /* lifetimes vary ±40% so bursts don't vanish at once */
#define SPEED_MIN        0.5f   /* each particle gets 50..100% of the burst speed */

/* How each kind looks and moves */
typedef struct {
    uint8_t r, g, b;
    float size;       /* meters */
    float gravity;    /* multiple of GRAVITY */
    float life;       /* seconds, before jitter */
} ParticleLook;

static const ParticleLook looks[PARTICLE_KIND_COUNT] = {
    [PARTICLE_BLOOD]  = {120, 8, 6,     0.05f, 1.0f, 0.8f},
    [PARTICLE_GIB]    = {88, 24, 18,    0.11f, 1.0f, 2.5f},
    [PARTICLE_SPARK]  = {255, 214, 130, 0.03f, 0.0f, 0.12f},
    [PARTICLE_DEBRIS] = {64, 54, 44,    0.07f, 1.0f, 1.5f},
    [PARTICLE_SPIT]   = {110, 220, 60,  0.05f, 1.0f, 0.6f},
};

/* --- Random numbers: tiny LCG, 0..1 --- */

#define LCG_MUL 1664525u
#define LCG_ADD 1013904223u

static float rnd01(uint32_t *seed) {
    *seed = *seed * LCG_MUL + LCG_ADD;
    return (float)(*seed >> 8) / (float)(1u << 24);
}

static float rnd_signed(uint32_t *seed) {
    return rnd01(seed) * 2.0f - 1.0f;
}

void particles_init(ParticleSystem *ps, uint32_t seed) {
    ps->count = 0;
    ps->seed = seed;
}

void particles_burst(ParticleSystem *ps, ParticleKind kind, Vec3 at, Vec3 dir,
                     int count, float speed, float spread, float floor_y) {
    Vec3 base = vec3_normalize(dir);
    const ParticleLook *look = &looks[kind];

    for (int i = 0; i < count && ps->count < PARTICLES_MAX; i++) {
        uint32_t *s = &ps->seed;
        Vec3 scatter = vec3(rnd_signed(s), rnd_signed(s), rnd_signed(s));
        Vec3 v = vec3_normalize(vec3_add(base, vec3_mul(scatter, spread)));
        float k = SPEED_MIN + (1.0f - SPEED_MIN) * rnd01(s);

        ps->items[ps->count++] = (Particle){
            .pos = at,
            .vel = vec3_mul(v, speed * k),
            .life = look->life * (1.0f + LIFE_JITTER * rnd_signed(s)),
            .floor_y = floor_y,
            .kind = kind,
        };
    }
}

void particles_update(ParticleSystem *ps, float dt) {
    for (int i = 0; i < ps->count;) {
        Particle *p = &ps->items[i];
        p->life -= dt;
        if (p->life <= 0) {
            ps->items[i] = ps->items[--ps->count];
            continue;
        }

        p->vel.y -= GRAVITY * looks[p->kind].gravity * dt;
        p->pos = vec3_add(p->pos, vec3_mul(p->vel, dt));

        /* Landed: stick to the floor (blood and gibs lie where they fall) */
        if (p->pos.y <= p->floor_y) {
            p->pos.y = p->floor_y;
            p->vel = vec3(0, 0, 0);
        }
        i++;
    }
}

/* One square of side px centered on (cx, cy), only where nearer than the scene */
static void splat(Framebuffer *fb, int cx, int cy, int px, float dist, uint32_t color) {
    int half = px / 2;
    for (int y = cy - half; y < cy - half + px; y++) {
        for (int x = cx - half; x < cx - half + px; x++) {
            if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) {
                continue;
            }
            int i = y * SCREEN_W + x;
            if (dist < fb->depth[i]) {
                fb->pixels[i] = color;
            }
        }
    }
}

void particles_draw(const ParticleSystem *ps, Framebuffer *fb, const Camera *cam,
                    const Environment *env) {
    float focal = camera_focal_px(cam);
    for (int i = 0; i < ps->count; i++) {
        const Particle *p = &ps->items[i];
        int x, y;
        float dist;
        if (!camera_project(cam, p->pos, &x, &y, &dist)) {
            continue;
        }

        const ParticleLook *look = &looks[p->kind];
        int px = (int)(look->size * focal / dist + 0.5f);
        px = px < SPLAT_MIN_PX ? SPLAT_MIN_PX : px > SPLAT_MAX_PX ? SPLAT_MAX_PX : px;

        /* Fade into the dark like the world's fog does */
        float fade = 1.0f - environment_fog(env, dist);
        uint32_t color = ps1_dither_color((uint8_t)(look->r * fade), (uint8_t)(look->g * fade),
                                          (uint8_t)(look->b * fade), x, y);
        splat(fb, x, y, px, dist, color);
    }
}
