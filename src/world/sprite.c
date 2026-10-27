#include "sprite.h"
#include <math.h>

#define SPRITE_EDGE_EPS 1e-4f /* keeps u/v off 1.0 so sampling never wraps */
#define SPRITE_VERTICAL 1e-6f /* rays this vertical can't hit an upright plane */

bool sprite_intersect(const Sprite *s, const Texture *tex, Ray ray,
                      float t_min, float t_max, HitRecord *rec) {
    /* Plane faces the ray horizontally; a straight up/down ray meets it
     * edge-on and never hits */
    Vec3 flat = vec3(ray.dir.x, 0, ray.dir.z);
    float flat_len = vec3_length(flat);
    if (flat_len < SPRITE_VERTICAL) {
        return false;
    }
    Vec3 normal = vec3_mul(flat, -1.0f / flat_len);

    float denom = vec3_dot(ray.dir, normal);
    float t = vec3_dot(vec3_sub(s->base, ray.origin), normal) / denom;
    if (t < t_min || t > t_max) {
        return false;
    }

    /* Local coordinates on the plane: across (right) and up from base */
    Vec3 p = vec3_add(ray.origin, vec3_mul(ray.dir, t));
    Vec3 right = vec3(normal.z, 0, -normal.x); /* viewer's right */
    float across = vec3_dot(vec3_sub(p, s->base), right) / s->width + 0.5f;
    float up = (p.y - s->base.y) / s->height;
    if (across < 0 || across >= 1 || up < 0 || up >= 1) {
        return false;
    }

    /* Texture rows run top-down; pick this sprite's frame column */
    int frames = s->frame_count > 0 ? s->frame_count : 1;
    float u = ((float)s->frame + fminf(across, 1 - SPRITE_EDGE_EPS)) / (float)frames;
    float v = fminf(1 - up, 1 - SPRITE_EDGE_EPS);
    if (tex && !texture_opaque(tex, u, v)) {
        return false;
    }

    *rec = (HitRecord){
        .t = t,
        .point = p,
        .normal = normal,
        .uv = {u, v},
        .material_id = s->material_id,
        .owner = s->owner,
        .hit = true,
    };
    return true;
}
