#include "render.h"
#include "scene.h"
#include "ps1_effects.h"
#include "threadpool.h"
#include <stddef.h>
#include <math.h>

void framebuffer_clear(Framebuffer *fb, uint32_t color) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        fb->pixels[i] = color;
    }
}

void framebuffer_set(Framebuffer *fb, int x, int y, uint32_t color) {
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) {
        fb->pixels[y * SCREEN_W + x] = color;
    }
}

uint32_t color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t)0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static Vec3 checkerboard(float u, float v) {
    int cu = (int)(u * 4.0f) % 2;
    int cv = (int)(v * 4.0f) % 2;
    if (u * 4.0f < 0) {
        cu = 1 - cu;
    }
    if (v * 4.0f < 0) {
        cv = 1 - cv;
    }
    return (cu ^ cv) ? vec3(0.9f, 0.9f, 0.9f) : vec3(0.2f, 0.2f, 0.2f);
}

static const Material *hit_material(const HitRecord *rec, const Material *materials,
                                    int mat_count) {
    bool valid = materials && rec->material_id >= 0 && rec->material_id < mat_count;
    return valid ? &materials[rec->material_id] : NULL;
}

/* Texture (scrolled by the material's motion) or flat color at uv */
static Vec3 get_surface_color(const Material *mat, Vec2 uv, float time,
                              const Texture *textures, int tex_count) {
    if (!mat) {
        return checkerboard(uv.u, uv.v);
    }
    if (mat->texture_id < 0 || mat->texture_id >= tex_count) {
        return mat->color;
    }
    float u = uv.u + mat->scroll.u * time;
    float v = uv.v + mat->scroll.v * time;
    return texture_sample(&textures[mat->texture_id], u, v);
}

#define PULSE_DEPTH 0.35f /* a pulsing glow dips to 65% */

/* A surface's own light (screens, slime), unaffected by the dark */
static float material_glow(const Material *mat, float time) {
    if (!mat || mat->glow <= 0) {
        return 0;
    }
    if (mat->pulse <= 0) {
        return mat->glow;
    }
    float wave = 0.5f + 0.5f * sinf(time * mat->pulse * 2.0f * (float)MATH_PI);
    return mat->glow * (1.0f - PULSE_DEPTH * wave);
}

/* --- Shared shading logic for lit rendering --- */

typedef struct {
    const Environment *env;   /* fog, sky (the view's) and sun (the level's) */
    const ZoneList *zones;    /* per-surface ambient */
    const Material *materials;
    const Texture *textures;
    int mat_count, tex_count;
    const Light *carried;   /* light moving with the viewer (flashlight); NULL = none */
    bool affine;            /* PS1 texture warp */
} RenderParams;

/* 2D cross product (z of the 3D one) */
static float cross2(float ax, float ay, float bx, float by) {
    return ax * by - ay * bx;
}

/* PS1 texture warp: the triangle's UVs interpolated straight across the
 * screen, without the perspective divide the console's rasterizer
 * skipped. Pixel p in screen triangle s0 s1 s2 = s0 + w1 (s1-s0) + w2 (s2-s0):
 *
 *        s2            w1, w2 from 2D cross products; the UV is then
 *        /\            uv0 + w1 (uv1-uv0) + w2 (uv2-uv0), as the
 *       / p\           PS1 drew it
 *    s0 ‾‾‾‾ s1
 *
 * False (keep the true UVs) if a vertex is behind the camera. */
static bool affine_uv(const Camera *cam, const Triangle *tri, int x, int y, Vec2 *uv) {
    float x0, y0, x1, y1, x2, y2;
    if (!camera_to_screen(cam, tri->v0, &x0, &y0) || !camera_to_screen(cam, tri->v1, &x1, &y1) ||
        !camera_to_screen(cam, tri->v2, &x2, &y2)) {
        return false;
    }
    float area = cross2(x1 - x0, y1 - y0, x2 - x0, y2 - y0);
    if (fabsf(area) < 1e-6f) {
        return false;
    }
    float px = (float)x + 0.5f - x0;
    float py = (float)y + 0.5f - y0;
    float w1 = cross2(px, py, x2 - x0, y2 - y0) / area;
    float w2 = cross2(x1 - x0, y1 - y0, px, py) / area;
    *uv = ps1_affine_uv(tri, w1, w2);
    return true;
}

/* One light's color on a surface point: cone, distance, facing, and a
 * shadow ray while the pixel's shadow budget (MAX_SHADOW_LIGHTS) lasts */
static Vec3 shade_light(const Light *pl, const Scene *scene, Vec3 hit_pos, Vec3 normal,
                        float time, int *shadow_count) {
    Vec3 none = vec3(0, 0, 0);
    Vec3 to_light = vec3_sub(pl->position, hit_pos);
    float dist = vec3_length(to_light);
    if (pl->off || dist >= pl->radius || dist < 1e-4f) {
        return none;
    }

    Vec3 light_dir = vec3_mul(to_light, 1.0f / dist);
    float ndl = vec3_dot(normal, light_dir);
    float cone = light_cone(pl, vec3_neg(light_dir));
    if (ndl <= 0 || cone <= 0) {
        return none;
    }

    if (pl->cast_shadows && *shadow_count < MAX_SHADOW_LIGHTS) {
        (*shadow_count)++;
        Ray shadow_ray = {vec3_add(hit_pos, vec3_mul(normal, 0.01f)), light_dir};
        if (scene_occluded(scene, shadow_ray, 0.001f, dist - 0.01f)) {
            return none;
        }
    }

    float amount = ndl * cone * light_attenuation(pl, dist) * light_effective_intensity(pl, time);
    return vec3_mul(pl->color, amount);
}

/* Shade one pixel; writes its hit distance (RENDER_FAR for sky) to *depth */
static uint32_t shade_pixel_lit(int x, int y, float *depth, const Camera *cam,
                                const Scene *scene, float time,
                                const RenderParams *params) {
    Ray ray = camera_ray(cam, x, y);
    HitRecord rec = scene_intersect(scene, ray, 0.001f, RENDER_FAR);
    *depth = rec.hit ? rec.t : RENDER_FAR;

    Vec3 col;
    if (rec.hit) {
        const Material *mat = hit_material(&rec, params->materials, params->mat_count);
        Vec2 uv = rec.uv;
        if (params->affine && rec.tri) {
            affine_uv(cam, rec.tri, x, y, &uv);
        }
        Vec3 surface_col = get_surface_color(mat, uv, time, params->textures,
                                             params->tex_count);
        Vec3 hit_pos = rec.point;
        Vec3 normal = rec.normal;

        /* Flip normal on back-face hits for double-sided lighting */
        if (vec3_dot(ray.dir, normal) > 0) {
            normal = vec3_mul(normal, -1.0f);
        }

        const Environment *env = params->env;
        float ambient = environment_ambient_at(&scene->env, params->zones, hit_pos) +
                        material_glow(mat, time);
        Vec3 lighting = vec3(ambient, ambient, ambient);

        /* Directional sun light with shadow; no sun, no shadow ray */
        float sun_ndl = vec3_dot(normal, env->sun_dir);
        if (env->sun_intensity > 0 && sun_ndl > 0) {
            Ray shadow_ray = {vec3_add(hit_pos, vec3_mul(normal, 0.01f)), env->sun_dir};
            if (!scene_occluded(scene, shadow_ray, 0.001f, 100.0f)) {
                lighting = vec3_add(lighting, vec3_mul(env->sun_color, sun_ndl * env->sun_intensity));
            }
        }

        /* Lamps, flashes, then the light the viewer carries */
        int shadow_count = 0;
        for (int i = 0; i < scene->lights.count; i++) {
            lighting = vec3_add(lighting, shade_light(&scene->lights.items[i], scene, hit_pos,
                                                      normal, time, &shadow_count));
        }
        if (params->carried) {
            lighting = vec3_add(lighting, shade_light(params->carried, scene, hit_pos, normal,
                                                      time, &shadow_count));
        }

        col.x = surface_col.x * lighting.x;
        col.y = surface_col.y * lighting.y;
        col.z = surface_col.z * lighting.z;

        col = vec3_lerp(col, params->env->fog_color, environment_fog(params->env, rec.t));
    } else {
        col = environment_sky(params->env, ray.dir);
    }

    /* Light source billboards — draw visible glowing dots at lamp positions.
     * Temporary lights (muzzle flashes, explosions) have no bulb to show. */
    float hit_t = rec.hit ? rec.t : RENDER_FAR;
    for (int i = 0; i < scene->lights.count; i++) {
        const Light *pl = &scene->lights.items[i];
        if (pl->ttl_total > 0 || pl->off) {
            continue;
        }
        Vec3 to_light = vec3_sub(pl->position, ray.origin);
        float t_along = vec3_dot(to_light, ray.dir);
        if (t_along <= 0.1f || t_along >= hit_t) {
            continue;
        }

        /* Distance from ray to light center */
        Vec3 closest = vec3_add(ray.origin, vec3_mul(ray.dir, t_along));
        Vec3 diff = vec3_sub(closest, pl->position);
        float dist_sq = vec3_dot(diff, diff);

        /* Apparent radius shrinks with distance (billboard) */
        float glow_radius = light_bulb_radius(pl);
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

static RenderParams make_render_params(const Scene *scene, const RenderView *view) {
    RenderView none = {0};
    view = view ? view : &none;
    return (RenderParams){
        .env       = view->env ? view->env : &scene->env,
        .zones     = &scene->zones,
        .materials = scene->materials,
        .textures  = scene->textures,
        .mat_count = scene->material_count,
        .tex_count = scene->texture_count,
        .carried   = view->carried,
        .affine    = view->affine,
    };
}

#define CARRIED_SPILL 0.14f /* share of a carried light falling on what you hold */

float render_light_at(const Scene *scene, Vec3 p, float time, const Light *carried) {
    float light = environment_ambient_at(&scene->env, &scene->zones, p);
    for (int i = 0; i < scene->lights.count; i++) {
        const Light *pl = &scene->lights.items[i];
        float dist = vec3_length(vec3_sub(pl->position, p));
        if (dist >= pl->radius) {
            continue;
        }
        light += light_attenuation(pl, dist) * light_effective_intensity(pl, time);
    }
    if (carried) {
        light += light_effective_intensity(carried, time) * CARRIED_SPILL;
    }
    return light;
}

/* --- Single-threaded lit render --- */

void render_scene_lit(Framebuffer *fb, const Camera *cam, const Scene *scene,
                      float time, const RenderView *view) {
    RenderParams params = make_render_params(scene, view);

    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            int i = y * SCREEN_W + x;
            fb->pixels[i] = shade_pixel_lit(x, y, &fb->depth[i], cam, scene, time, &params);
        }
    }
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
    for (int y = chunk->y_start; y < chunk->y_end; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            int i = y * SCREEN_W + x;
            chunk->fb->pixels[i] =
                shade_pixel_lit(x, y, &chunk->fb->depth[i], chunk->cam, chunk->scene,
                                chunk->time, &chunk->params);
        }
    }
}

void render_scene_lit_mt(Framebuffer *fb, const Camera *cam, const Scene *scene,
                         float time, ThreadPool *pool, const RenderView *view) {
    int n = pool->worker_count;
    RenderChunk chunks[THREADPOOL_MAX_WORKERS];
    void *args[THREADPOOL_MAX_WORKERS];
    RenderParams params = make_render_params(scene, view);

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
