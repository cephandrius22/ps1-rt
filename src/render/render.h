/*
 * render.h — Raytraced scene rendering to a 320x240 framebuffer.
 *
 * Per pixel: camera ray → scene_intersect() (static + dynamic BVH) →
 * material/texture → ambient + sun + scene lights (shadow rays) + the
 * light the viewer carries (flashlight), all through one light model
 * (light.h) → distance fog → light billboards → PS1 dither. Ambient,
 * sun, fog and sky come from the level (scene->env, environment.h).
 * Output is ARGB, ready for SDL texture upload.
 *
 * Reads world/ for *what* is in the scene (geometry, materials, lights)
 * and gfx/ for *how* to draw it (camera rays, texture sampling, dither).
 */
#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include <stdbool.h>
#include "camera.h"
#include "light.h"

/* Forward declare — full definition in scene.h */
struct Scene;

#define RENDER_FAR       1000.0f /* depth of sky pixels; max ray distance */

/* Color plus per-pixel distance from the eye (along the pixel's ray), so
 * things drawn after the raytrace (particles) can hide behind walls */
typedef struct {
    uint32_t pixels[SCREEN_W * SCREEN_H];
    float depth[SCREEN_W * SCREEN_H];
} Framebuffer;

/* Pixels are ARGB 0xAARRGGBB; framebuffer_set ignores out-of-bounds */
void framebuffer_clear(Framebuffer *fb, uint32_t color);
void framebuffer_set(Framebuffer *fb, int x, int y, uint32_t color);
uint32_t color_rgb(uint8_t r, uint8_t g, uint8_t b);

/* Rough brightness at a point (no shadows): ambient + scene lights + a
 * share of the carried light. Lights the viewmodel like its surroundings. */
float render_light_at(const struct Scene *scene, Vec3 p, float time, const Light *carried);

/* Full scene render. carried: a light moving with the viewer this frame
 * (the flashlight), shaded like scene lights; NULL = none. */
void render_scene_lit(Framebuffer *fb, const Camera *cam, const struct Scene *scene,
                      float time, const Light *carried);

/* Multithreaded version — splits scanlines across thread pool workers */
struct ThreadPool;
void render_scene_lit_mt(Framebuffer *fb, const Camera *cam, const struct Scene *scene,
                         float time, struct ThreadPool *pool, const Light *carried);

#endif
