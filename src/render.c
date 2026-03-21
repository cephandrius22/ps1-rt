#include "render.h"
#include "scene.h"
#include <stddef.h>

void framebuffer_clear(Framebuffer *fb, uint32_t color) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
        fb->pixels[i] = color;
}

void framebuffer_set(Framebuffer *fb, int x, int y, uint32_t color) {
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
        fb->pixels[y * SCREEN_W + x] = color;
}

uint32_t color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t)0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

Vec3 sky_color(Vec3 dir) {
    float t = 0.5f * (dir.y + 1.0f);
    Vec3 sky_top = vec3(0.15f, 0.05f, 0.25f);
    Vec3 sky_bot = vec3(0.6f, 0.3f, 0.1f);
    return vec3_lerp(sky_bot, sky_top, t);
}

static Vec3 checkerboard(float u, float v) {
    int cu = (int)(u * 4.0f) % 2;
    int cv = (int)(v * 4.0f) % 2;
    if (u * 4.0f < 0) cu = 1 - cu;
    if (v * 4.0f < 0) cv = 1 - cv;
    return (cu ^ cv) ? vec3(0.9f, 0.9f, 0.9f) : vec3(0.2f, 0.2f, 0.2f);
}

static Vec3 get_surface_color(const HitRecord *rec,
                              const Material *materials, int mat_count,
                              const Texture *textures, int tex_count) {
    if (materials && rec->material_id >= 0 && rec->material_id < mat_count) {
        const Material *mat = &materials[rec->material_id];
        if (mat->texture_id >= 0 && mat->texture_id < tex_count)
            return texture_sample(&textures[mat->texture_id], rec->uv.u, rec->uv.v);
        return mat->color;
    }
    return checkerboard(rec->uv.u, rec->uv.v);
}

/* Original render (no point lights, uses single BVH) */
void render_scene(Framebuffer *fb, const Camera *cam, const BVH *bvh, const SceneMaterials *mats) {
    Vec3 light_dir = vec3_normalize(vec3(0.5f, 1.0f, -0.3f));
    float ambient = 0.15f;
    float fog_start = 10.0f;
    float fog_end = 30.0f;
    Vec3 fog_color = vec3(0.25f, 0.12f, 0.35f);

    const Material *materials = mats ? mats->materials : NULL;
    const Texture *textures = mats ? mats->textures : NULL;
    int mat_count = mats ? mats->material_count : 0;
    int tex_count = mats ? mats->texture_count : 0;

    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            Ray ray = camera_ray(cam, x, y);
            HitRecord rec = bvh_intersect(bvh, ray, 0.001f, 1000.0f);

            Vec3 col;
            if (rec.hit) {
                Vec3 surface_col = get_surface_color(&rec, materials, mat_count, textures, tex_count);

                float ndl = vec3_dot(rec.normal, light_dir);
                if (ndl < 0) ndl = 0;
                float intensity = ambient + (1.0f - ambient) * ndl;
                col.x = surface_col.x * intensity;
                col.y = surface_col.y * intensity;
                col.z = surface_col.z * intensity;

                float fog_t = clampf((rec.t - fog_start) / (fog_end - fog_start), 0, 1);
                col = vec3_lerp(col, fog_color, fog_t);
            } else {
                col = sky_color(ray.dir);
            }

            uint8_t cr = (uint8_t)(clampf(col.x, 0, 1) * 255.0f);
            uint8_t cg = (uint8_t)(clampf(col.y, 0, 1) * 255.0f);
            uint8_t cb = (uint8_t)(clampf(col.z, 0, 1) * 255.0f);
            framebuffer_set(fb, x, y, ps1_dither_color(cr, cg, cb, x, y));
        }
    }
}

/* Full scene render with point lights and shadow rays */
void render_scene_lit(Framebuffer *fb, const Camera *cam, const Scene *scene, float time) {
    Vec3 sun_dir = vec3_normalize(vec3(0.5f, 1.0f, -0.3f));
    float ambient = 0.08f;
    float fog_start = 10.0f;
    float fog_end = 30.0f;
    Vec3 fog_color = vec3(0.25f, 0.12f, 0.35f);

    const Material *materials = scene->materials;
    const Texture *textures = scene->textures;
    int mat_count = scene->material_count;
    int tex_count = scene->texture_count;

    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            Ray ray = camera_ray(cam, x, y);
            HitRecord rec = scene_intersect(scene, ray, 0.001f, 1000.0f);

            Vec3 col;
            if (rec.hit) {
                Vec3 surface_col = get_surface_color(&rec, materials, mat_count, textures, tex_count);
                Vec3 hit_pos = rec.point;
                Vec3 normal = rec.normal;

                /* Start with ambient */
                Vec3 lighting = vec3(ambient, ambient, ambient);

                /* Directional sun light */
                float sun_ndl = vec3_dot(normal, sun_dir);
                if (sun_ndl > 0) {
                    Ray shadow_ray = {vec3_add(hit_pos, vec3_mul(normal, 0.01f)), sun_dir};
                    if (!scene_occluded(scene, shadow_ray, 0.001f, 100.0f)) {
                        lighting = vec3_add(lighting, vec3_mul(vec3(1, 1, 1), sun_ndl * 0.5f));
                    }
                }

                /* Point lights — find nearest shadow-casting lights */
                int shadow_count = 0;
                for (int i = 0; i < scene->lights.count; i++) {
                    const PointLight *pl = &scene->lights.items[i];
                    Vec3 to_light = vec3_sub(pl->position, hit_pos);
                    float dist = vec3_length(to_light);

                    if (dist >= pl->radius) continue;

                    Vec3 light_dir = vec3_mul(to_light, 1.0f / dist);
                    float ndl = vec3_dot(normal, light_dir);
                    if (ndl <= 0) continue;

                    float atten = light_attenuation(pl, dist);
                    float eff_intensity = light_effective_intensity(pl, time);
                    float contribution = ndl * atten * eff_intensity;

                    /* Shadow ray (limited budget) */
                    if (pl->cast_shadows && shadow_count < MAX_SHADOW_LIGHTS) {
                        Ray shadow_ray = {vec3_add(hit_pos, vec3_mul(normal, 0.01f)), light_dir};
                        if (scene_occluded(scene, shadow_ray, 0.001f, dist - 0.01f)) {
                            shadow_count++;
                            continue;
                        }
                        shadow_count++;
                    }

                    lighting.x += pl->color.x * contribution;
                    lighting.y += pl->color.y * contribution;
                    lighting.z += pl->color.z * contribution;
                }

                col.x = surface_col.x * lighting.x;
                col.y = surface_col.y * lighting.y;
                col.z = surface_col.z * lighting.z;

                float fog_t = clampf((rec.t - fog_start) / (fog_end - fog_start), 0, 1);
                col = vec3_lerp(col, fog_color, fog_t);
            } else {
                col = sky_color(ray.dir);
            }

            uint8_t cr = (uint8_t)(clampf(col.x, 0, 1) * 255.0f);
            uint8_t cg = (uint8_t)(clampf(col.y, 0, 1) * 255.0f);
            uint8_t cb = (uint8_t)(clampf(col.z, 0, 1) * 255.0f);
            framebuffer_set(fb, x, y, ps1_dither_color(cr, cg, cb, x, y));
        }
    }
}
