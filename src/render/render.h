/*
 * render.h — Raytraced scene rendering to a 320x240 framebuffer.
 *
 * Per pixel: camera ray → scene_intersect() (static + dynamic BVH) →
 * material/texture → sun + point lights (shadow rays) + flashlight →
 * distance fog → light billboards → PS1 dither. Output is ARGB, ready
 * for SDL texture upload.
 *
 * Reads world/ for *what* is in the scene (geometry, materials, lights)
 * and gfx/ for *how* to draw it (camera rays, texture sampling, dither).
 */
#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include <stdbool.h>
#include "camera.h"

/* Forward declare — full definition in scene.h */
struct Scene;

#define RENDER_FAR       1000.0f /* depth of sky pixels; max ray distance */
#define RENDER_FOG_START 8.0f    /* fog begins (meters) */
#define RENDER_FOG_END   22.0f   /* fully fogged */

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

/* Background for rays that hit nothing: dim vertical gradient */
Vec3 sky_color(Vec3 dir);

/* Rough brightness at a point (no shadows): ambient + point lights +
 * flashlight. Used to light the viewmodel like its surroundings. */
float render_light_at(const struct Scene *scene, Vec3 p, float time, bool flashlight);

/* Full scene render with point lights and shadow rays */
void render_scene_lit(Framebuffer *fb, const Camera *cam, const struct Scene *scene,
                      float time, bool flashlight);

/* Multithreaded version — splits scanlines across thread pool workers */
struct ThreadPool;
void render_scene_lit_mt(Framebuffer *fb, const Camera *cam, const struct Scene *scene,
                         float time, struct ThreadPool *pool, bool flashlight);

#endif
