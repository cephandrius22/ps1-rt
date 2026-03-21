#include "render.h"

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

void render_scene(Framebuffer *fb, const Camera *cam, const BVH *bvh, const SceneMaterials *mats) {
    Vec3 light_dir = vec3_normalize(vec3(0.5f, 1.0f, -0.3f));
    float ambient = 0.15f;
    float fog_start = 10.0f;
    float fog_end = 30.0f;
    Vec3 fog_color = vec3(0.25f, 0.12f, 0.35f);

    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            Ray ray = camera_ray(cam, x, y);
            HitRecord rec = bvh_intersect(bvh, ray, 0.001f, 1000.0f);

            Vec3 col;
            if (rec.hit) {
                Vec3 surface_col = vec3(0.8f, 0.8f, 0.8f);
                if (mats && rec.material_id >= 0 && rec.material_id < mats->material_count) {
                    const Material *mat = &mats->materials[rec.material_id];
                    if (mat->texture_id >= 0 && mat->texture_id < mats->texture_count) {
                        surface_col = texture_sample(&mats->textures[mat->texture_id], rec.uv.u, rec.uv.v);
                    } else {
                        surface_col = mat->color;
                    }
                } else {
                    surface_col = checkerboard(rec.uv.u, rec.uv.v);
                }

                float ndl = vec3_dot(rec.normal, light_dir);
                if (ndl < 0) ndl = 0;
                float intensity = ambient + (1.0f - ambient) * ndl;
                col.x = surface_col.x * intensity;
                col.y = surface_col.y * intensity;
                col.z = surface_col.z * intensity;

                // Distance fog
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
