/*
 * particles.h — Blood, gibs, sparks and debris: cheap cosmetic specks.
 *
 * Particles are many and tiny, so they are NOT ray-traced (a sprite costs
 * per pixel per ray). After the raytrace, each particle is projected to
 * the screen and splatted as a small square, hidden by the depth buffer:
 *
 *   raytrace ──> color + depth ──> particles_draw(): project, depth-test,
 *                                  1–4 px square ──> overlay
 *
 * Presentation only: spawned by main.c from game events, moved with frame
 * time, never touch gameplay. They land on a flat floor height given at
 * spawn (good enough for flat levels; no scene collision).
 */
#ifndef PARTICLES_H
#define PARTICLES_H

#include "render.h"

typedef enum {
    PARTICLE_BLOOD,
    PARTICLE_GIB,
    PARTICLE_SPARK,
    PARTICLE_DEBRIS,
    PARTICLE_SPIT,     /* green droplets where spit lands */
    PARTICLE_KIND_COUNT
} ParticleKind;

typedef struct {
    Vec3 pos;
    Vec3 vel;
    float life;      /* seconds left */
    float floor_y;   /* lands (and stays) here */
    ParticleKind kind;
} Particle;

#define PARTICLES_MAX 512

typedef struct {
    Particle items[PARTICLES_MAX];
    int count;
    uint32_t seed;   /* own RNG: deterministic tests, no shared rand() state */
} ParticleSystem;

/* Empty system; the seed makes bursts reproducible (tests pass a constant) */
void particles_init(ParticleSystem *ps, uint32_t seed);

/* Spray count particles from at, mostly along dir (need not be unit),
 * scattered by spread (0 = straight, 1 = wide cone). Full = dropped. */
void particles_burst(ParticleSystem *ps, ParticleKind kind, Vec3 at, Vec3 dir,
                     int count, float speed, float spread, float floor_y);

/* Gravity, landing, aging; dead particles are swap-removed */
void particles_update(ParticleSystem *ps, float dt);

/* Splat into fb behind the depth buffer, darker with distance (fog) */
void particles_draw(const ParticleSystem *ps, Framebuffer *fb, const Camera *cam);

#endif
