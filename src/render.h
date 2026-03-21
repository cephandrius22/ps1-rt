/*
 * render.h — Scene rendering to a 320x240 framebuffer.
 *
 * For each pixel: generates a ray, intersects the BVH, applies material/texture
 * lookup, directional lighting, distance fog, and PS1 dithering. Outputs ARGB
 * pixel data ready for SDL2 texture upload.
 */
#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include "camera.h"
#include "mesh.h"
#include "bvh.h"
#include "texture.h"
#include "ps1_effects.h"

typedef struct {
    uint32_t pixels[SCREEN_W * SCREEN_H];
} Framebuffer;

typedef struct {
    Material *materials;
    Texture *textures;
    int material_count;
    int texture_count;
} SceneMaterials;

void framebuffer_clear(Framebuffer *fb, uint32_t color);
void framebuffer_set(Framebuffer *fb, int x, int y, uint32_t color);
uint32_t color_rgb(uint8_t r, uint8_t g, uint8_t b);
Vec3 sky_color(Vec3 dir);
void render_scene(Framebuffer *fb, const Camera *cam, const BVH *bvh, const SceneMaterials *mats);

#endif
