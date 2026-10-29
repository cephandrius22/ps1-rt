#include "test_framework.h"
#include "shadowcache.h"

/* A 10 m floor (owner 1) with a 2 m roof slab 1 m above its middle
 * (owner 2), and a light 3 m up: the slab shades the floor below it */
static void slab_scene(BVH *bvh) {
    Vec3 up = vec3(0, 1, 0);
    Triangle tris[] = {
        {vec3(-5, 0, -5), vec3(5, 0, -5), vec3(5, 0, 5), .normal = up, .owner = 1},
        {vec3(-5, 0, -5), vec3(5, 0, 5), vec3(-5, 0, 5), .normal = up, .owner = 1},
        {vec3(-1, 1, -1), vec3(1, 1, -1), vec3(1, 1, 1), .normal = up, .owner = 2},
        {vec3(-1, 1, -1), vec3(1, 1, 1), vec3(-1, 1, 1), .normal = up, .owner = 2},
    };
    bvh_build(bvh, tris, 4);
}

static Light lamp(Vec3 at) {
    return (Light){.position = at, .color = vec3(1, 1, 1), .intensity = 1,
                   .radius = 12, .cast_shadows = true};
}

/* The floor triangle under p (index in the BVH), found from below */
static int floor_tri(const BVH *bvh, Vec3 p) {
    Ray up = {vec3(p.x, -1, p.z), vec3(0, 1, 0)};
    HitRecord rec = bvh_intersect(bvh, up, 0.001f, 10);
    return rec.hit ? (int)(rec.tri - bvh->tris) : -1;
}

TEST(test_shadowcache_slab_shades_floor) {
    BVH bvh;
    slab_scene(&bvh);
    LightList lights;
    light_list_init(&lights);
    light_add(&lights, lamp(vec3(0, 3, 0)));

    ShadowCache c;
    shadowcache_build(&c, &bvh, &lights);
    ASSERT(shadowcache_has(&c, 0));

    Vec3 under = vec3(0.2f, 0, -0.3f);
    Vec3 open = vec3(3.5f, 0, 3.5f);
    ASSERT_NEAR(shadowcache_visible(&c, floor_tri(&bvh, under), under, 0), 0.0f, 1e-6f);
    ASSERT_NEAR(shadowcache_visible(&c, floor_tri(&bvh, open), open, 0), 1.0f, 1e-6f);

    /* Across the shadow's edge (x = 1.5 under a light at 3 m over a slab
     * at 1 m), visibility rises from 0 to 1 over about one texel */
    Vec3 inside = vec3(1.2f, 0, 0.1f);
    Vec3 outside = vec3(1.8f, 0, 0.1f);
    ASSERT(shadowcache_visible(&c, floor_tri(&bvh, inside), inside, 0) < 0.5f);
    ASSERT(shadowcache_visible(&c, floor_tri(&bvh, outside), outside, 0) > 0.5f);

    shadowcache_free(&c);
    light_list_free(&lights);
    bvh_free(&bvh);
}

/* Only fixed, permanent, shadow-casting lights in the first 32 slots */
TEST(test_shadowcache_which_lights) {
    BVH bvh;
    slab_scene(&bvh);
    LightList lights;
    light_list_init(&lights);

    Light swinging = lamp(vec3(0, 3, 0));
    swinging.swing_speed = 1;
    Light unshadowed = lamp(vec3(0, 3, 0));
    unshadowed.cast_shadows = false;
    light_add(&lights, lamp(vec3(0, 3, 0)));
    light_add(&lights, swinging);
    light_add(&lights, unshadowed);
    for (int i = 3; i < SHADOW_LIGHTS_MAX + 2; i++) {
        light_add(&lights, lamp(vec3(0, 3, 0)));
    }

    ShadowCache c;
    shadowcache_build(&c, &bvh, &lights);
    ASSERT(shadowcache_has(&c, 0));
    ASSERT(!shadowcache_has(&c, 1));
    ASSERT(!shadowcache_has(&c, 2));
    ASSERT(shadowcache_has(&c, SHADOW_LIGHTS_MAX - 1));
    ASSERT(!shadowcache_has(&c, SHADOW_LIGHTS_MAX));
    ASSERT(!shadowcache_has(&c, -1));

    shadowcache_free(&c);
    light_list_free(&lights);
    bvh_free(&bvh);
}

int main(void) {
    TEST_SUITE("shadowcache");
    RUN_TEST(test_shadowcache_slab_shades_floor);
    RUN_TEST(test_shadowcache_which_lights);
    TEST_REPORT();
}
