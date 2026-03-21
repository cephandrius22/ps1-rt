#include "render.h"
#include "scene.h"
#include "threadpool.h"
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

/* --- Shared shading logic for lit rendering --- */

typedef struct {
    Vec3 sun_dir;
    float ambient;
    float fog_start, fog_end;
    Vec3 fog_color;
    const Material *materials;
    const Texture *textures;
    int mat_count, tex_count;
    bool flashlight;
    Vec3 flashlight_pos;
    Vec3 flashlight_dir;
} RenderParams;

static uint32_t shade_pixel_lit(int x, int y, const Camera *cam,
                                const Scene *scene, float time,
                                const RenderParams *params) {
    Ray ray = camera_ray(cam, x, y);
    HitRecord rec = scene_intersect(scene, ray, 0.001f, 1000.0f);

    Vec3 col;
    if (rec.hit) {
        Vec3 surface_col = get_surface_color(&rec, params->materials,
                                             params->mat_count, params->textures,
                                             params->tex_count);
        Vec3 hit_pos = rec.point;
        Vec3 normal = rec.normal;

        /* Flip normal on back-face hits for double-sided lighting */
        if (vec3_dot(ray.dir, normal) > 0)
            normal = vec3_mul(normal, -1.0f);

        Vec3 lighting = vec3(params->ambient, params->ambient, params->ambient);

        /* Directional sun light with shadow */
        float sun_ndl = vec3_dot(normal, params->sun_dir);
        if (sun_ndl > 0) {
            Ray shadow_ray = {vec3_add(hit_pos, vec3_mul(normal, 0.01f)), params->sun_dir};
            if (!scene_occluded(scene, shadow_ray, 0.001f, 100.0f))
                lighting = vec3_add(lighting, vec3_mul(vec3(1, 1, 1), sun_ndl * 0.5f));
        }

        /* Point lights */
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

        /* Flashlight (player-held spotlight) */
        if (params->flashlight) {
            Vec3 to_hit = vec3_sub(hit_pos, params->flashlight_pos);
            float dist = vec3_length(to_hit);
            if (dist > 0.01f && dist < 20.0f) {
                Vec3 light_dir = vec3_mul(to_hit, 1.0f / dist);
                float spot = vec3_dot(light_dir, params->flashlight_dir);
                /* Cone: ~25 degree half-angle (cos 25 ~ 0.9) */
                if (spot > 0.9f) {
                    float ndl = vec3_dot(normal, vec3_mul(light_dir, -1.0f));
                    if (ndl > 0) {
                        float falloff = (spot - 0.9f) / 0.1f; /* 0 at edge, 1 at center */
                        falloff *= falloff;
                        float atten = 1.0f / (1.0f + dist * 0.15f);
                        float contrib = ndl * falloff * atten * 2.5f;
                        lighting.x += contrib;
                        lighting.y += contrib;
                        lighting.z += contrib * 0.9f; /* slightly warm */
                    }
                }
            }
        }

        col.x = surface_col.x * lighting.x;
        col.y = surface_col.y * lighting.y;
        col.z = surface_col.z * lighting.z;

        float fog_t = clampf((rec.t - params->fog_start) /
                             (params->fog_end - params->fog_start), 0, 1);
        col = vec3_lerp(col, params->fog_color, fog_t);
    } else {
        col = sky_color(ray.dir);
    }

    /* Light source billboards — draw visible glowing dots at light positions */
    float hit_t = rec.hit ? rec.t : 1000.0f;
    for (int i = 0; i < scene->lights.count; i++) {
        const PointLight *pl = &scene->lights.items[i];
        Vec3 to_light = vec3_sub(pl->position, ray.origin);
        float t_along = vec3_dot(to_light, ray.dir);
        if (t_along <= 0.1f || t_along >= hit_t) continue;

        /* Distance from ray to light center */
        Vec3 closest = vec3_add(ray.origin, vec3_mul(ray.dir, t_along));
        Vec3 diff = vec3_sub(closest, pl->position);
        float dist_sq = vec3_dot(diff, diff);

        /* Apparent radius shrinks with distance (billboard) */
        float glow_radius = 0.08f + 0.02f * pl->intensity;
        if (dist_sq < glow_radius * glow_radius) {
            float eff = light_effective_intensity(pl, time);
            float brightness = 2.0f * eff;
            col.x = pl->color.x * brightness;
            col.y = pl->color.y * brightness;
            col.z = pl->color.z * brightness;
            break;
        }
    }

    uint8_t cr = (uint8_t)(clampf(col.x, 0, 1) * 255.0f);
    uint8_t cg = (uint8_t)(clampf(col.y, 0, 1) * 255.0f);
    uint8_t cb = (uint8_t)(clampf(col.z, 0, 1) * 255.0f);
    return ps1_dither_color(cr, cg, cb, x, y);
}

static RenderParams make_render_params(const Scene *scene, const Camera *cam,
                                       bool flashlight) {
    RenderParams p = {
        .sun_dir   = vec3_normalize(vec3(0.5f, 1.0f, -0.3f)),
        .ambient   = 0.12f,
        .fog_start = 10.0f,
        .fog_end   = 30.0f,
        .fog_color = vec3(0.25f, 0.12f, 0.35f),
        .materials = scene->materials,
        .textures  = scene->textures,
        .mat_count = scene->material_count,
        .tex_count = scene->texture_count,
        .flashlight = flashlight,
    };
    if (flashlight) {
        p.flashlight_pos = cam->position;
        /* Compute camera forward direction */
        Mat4 rot = mat4_mul(mat4_rotation_y(cam->yaw), mat4_rotation_x(cam->pitch));
        p.flashlight_dir = vec3_normalize(mat4_mul_vec3(rot, vec3(0, 0, -1), 0.0f));
    }
    return p;
}

/* --- Original single-BVH render (kept for tests) --- */

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

/* --- Single-threaded lit render --- */

void render_scene_lit(Framebuffer *fb, const Camera *cam, const Scene *scene,
                      float time, bool flashlight) {
    RenderParams params = make_render_params(scene, cam, flashlight);

    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            fb->pixels[y * SCREEN_W + x] = shade_pixel_lit(x, y, cam, scene, time, &params);
}

/* --- Multithreaded lit render --- */

typedef struct {
    Framebuffer *fb;
    const Camera *cam;
    const Scene *scene;
    float time;
    RenderParams params;
    int y_start;
    int y_end;
} RenderChunk;

static void render_chunk_func(void *arg) {
    RenderChunk *chunk = arg;
    for (int y = chunk->y_start; y < chunk->y_end; y++)
        for (int x = 0; x < SCREEN_W; x++)
            chunk->fb->pixels[y * SCREEN_W + x] =
                shade_pixel_lit(x, y, chunk->cam, chunk->scene,
                                chunk->time, &chunk->params);
}

void render_scene_lit_mt(Framebuffer *fb, const Camera *cam, const Scene *scene,
                         float time, ThreadPool *pool, bool flashlight) {
    int n = pool->worker_count;
    RenderChunk chunks[THREADPOOL_MAX_WORKERS];
    void *args[THREADPOOL_MAX_WORKERS];
    RenderParams params = make_render_params(scene, cam, flashlight);

    int rows_per = SCREEN_H / n;
    for (int i = 0; i < n; i++) {
        chunks[i].fb = fb;
        chunks[i].cam = cam;
        chunks[i].scene = scene;
        chunks[i].time = time;
        chunks[i].params = params;
        chunks[i].y_start = i * rows_per;
        chunks[i].y_end = (i == n - 1) ? SCREEN_H : (i + 1) * rows_per;
        args[i] = &chunks[i];
    }

    threadpool_dispatch(pool, render_chunk_func, args, n);
}
