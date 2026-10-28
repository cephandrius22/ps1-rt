#include "movement.h"
#include <math.h>

#define GROUND_MIN_NORMAL 0.5f   /* steeper surfaces are walls, not floor */
#define GROUND_PROBE_PAD  0.05f  /* extra lift on the ground probe origin */
#define GROUND_SKIP_MAX   4      /* steep surfaces to look past for real ground */
#define GROUND_SKIP_EPS   1e-3f
#define WALL_PROBE_ABOVE  0.05f  /* lowest wall probe sits this far above `step` */
#define WALL_PROBE_HEAD   0.1f   /* top probe sits this far below the head */
#define MIN_MOVE          1e-6f
#define WALL_TRACE_EPS    1e-3f  /* skip t≈0: an enemy's probes start on its own sprite */

float movement_ground(const Scene *s, Vec3 feet, float step, float max_drop) {
    float lift = step + GROUND_PROBE_PAD;
    Ray down = {vec3(feet.x, feet.y + lift, feet.z), vec3(0, -1, 0)};
    float t_min = 0.0f;
    float t_max = lift + max_drop;

    for (int i = 0; i < GROUND_SKIP_MAX; i++) {
        HitRecord rec = scene_trace(s, down, t_min, t_max, TRACE_SOLID);
        if (!rec.hit) {
            return MOVEMENT_NO_GROUND;
        }
        if (rec.normal.y > GROUND_MIN_NORMAL) {
            return rec.point.y;
        }
        t_min = rec.t + GROUND_SKIP_EPS;
    }
    return MOVEMENT_NO_GROUND;
}

/* One axis: probe at step, waist and head height; stop radius short */
static Vec3 slide_axis(const Scene *s, Vec3 feet, Vec3 move, BodyShape shape) {
    float len = vec3_length(move);
    if (len < MIN_MOVE) {
        return feet;
    }

    Vec3 dir = vec3_mul(move, 1.0f / len);
    float low = shape.step + WALL_PROBE_ABOVE;
    float heights[] = {
        low,
        fmaxf(shape.height * 0.5f, low),
        fmaxf(shape.height - WALL_PROBE_HEAD, low),
    };

    for (int i = 0; i < 3; i++) {
        Ray ray = {vec3_add(feet, vec3(0, heights[i], 0)), dir};
        HitRecord hit = scene_trace(s, ray, WALL_TRACE_EPS, len + shape.radius, TRACE_SOLID);
        if (hit.hit) {
            len = fminf(len, fmaxf(hit.t - shape.radius, 0.0f));
        }
    }
    return vec3_add(feet, vec3_mul(dir, len));
}

Vec3 movement_slide(const Scene *s, Vec3 feet, Vec3 move, BodyShape shape) {
    feet = slide_axis(s, feet, vec3(move.x, 0, 0), shape);
    return slide_axis(s, feet, vec3(0, 0, move.z), shape);
}
