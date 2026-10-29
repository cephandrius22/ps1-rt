#include "test_framework.h"
#include "render.h"
#include "scene.h"
#include "camera.h"

static Framebuffer frame_a, frame_b; /* 300 KB each: keep off the stack */

TEST(test_color_rgb) {
    uint32_t c = color_rgb(0xFF, 0x80, 0x40);
    ASSERT((c >> 24) == 0xFF);              // Alpha
    ASSERT(((c >> 16) & 0xFF) == 0xFF);     // R
    ASSERT(((c >> 8) & 0xFF) == 0x80);      // G
    ASSERT((c & 0xFF) == 0x40);             // B
}

TEST(test_color_rgb_black) {
    uint32_t c = color_rgb(0, 0, 0);
    ASSERT(c == 0xFF000000);
}

TEST(test_color_rgb_white) {
    uint32_t c = color_rgb(255, 255, 255);
    ASSERT(c == 0xFFFFFFFF);
}

TEST(test_framebuffer_set_and_read) {
    Framebuffer fb;
    framebuffer_clear(&fb, 0xFF000000);

    framebuffer_set(&fb, 10, 20, 0xFFFF0000);
    ASSERT(fb.pixels[20 * SCREEN_W + 10] == 0xFFFF0000);
}

TEST(test_framebuffer_clear) {
    Framebuffer fb;
    framebuffer_clear(&fb, 0xDEADBEEF);
    ASSERT(fb.pixels[0] == 0xDEADBEEF);
    ASSERT(fb.pixels[SCREEN_W * SCREEN_H - 1] == 0xDEADBEEF);
    ASSERT(fb.pixels[SCREEN_W * SCREEN_H / 2] == 0xDEADBEEF);
}

TEST(test_framebuffer_bounds_check) {
    Framebuffer fb;
    framebuffer_clear(&fb, 0);

    // These should be silently ignored (out of bounds)
    framebuffer_set(&fb, -1, 0, 0xFFFFFFFF);
    framebuffer_set(&fb, SCREEN_W, 0, 0xFFFFFFFF);
    framebuffer_set(&fb, 0, -1, 0xFFFFFFFF);
    framebuffer_set(&fb, 0, SCREEN_H, 0xFFFFFFFF);

    // Verify corners are still zero
    ASSERT(fb.pixels[0] == 0);
    ASSERT(fb.pixels[SCREEN_W - 1] == 0);
    ASSERT(fb.pixels[(SCREEN_H - 1) * SCREEN_W] == 0);
}

/* A floor quad (checkerboard UVs, no materials) from x0..x1, z0..z1 */
static void floor_quad(Scene *s, Mesh *m, float z_near) {
    scene_init(s);
    mesh_init(m);
    Vec3 a = vec3(-4, -1, z_near), b = vec3(4, -1, z_near);
    Vec3 c = vec3(4, -1, -20), d = vec3(-4, -1, -20);
    Vec3 up = vec3(0, 1, 0);
    mesh_add_tri(m, (Triangle){a, b, c, {0, 0}, {4, 0}, {4, 8}, up, -1, 0});
    mesh_add_tri(m, (Triangle){a, c, d, {0, 0}, {4, 8}, {0, 8}, up, -1, 0});
    scene_build_static(s, m->tris, m->count);
    s->env.ambient = 1.0f; /* lit evenly: only the texture pattern differs */
}

static int differing_pixels(void) {
    int n = 0;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        n += frame_a.pixels[i] != frame_b.pixels[i];
    }
    return n;
}

/* Affine UVs bend a receding floor's pattern; a triangle reaching behind
 * the camera keeps true UVs (its screen projection doesn't exist) */
TEST(test_render_affine_warp) {
    Scene s;
    Mesh m;
    Camera cam = camera_create(vec3(0, 0, 0), 0, 0, 1.2f);
    RenderView normal = {0}, affine = {.affine = true};

    floor_quad(&s, &m, -1.5f);
    render_scene_lit(&frame_a, &cam, &s, 0, &normal);
    render_scene_lit(&frame_b, &cam, &s, 0, &affine);
    ASSERT(differing_pixels() > 1000);
    mesh_free(&m);
    scene_free(&s);

    floor_quad(&s, &m, 5.0f);   /* starts behind the eye */
    render_scene_lit(&frame_a, &cam, &s, 0, &normal);
    render_scene_lit(&frame_b, &cam, &s, 0, &affine);
    ASSERT_EQ_INT(differing_pixels(), 0);
    mesh_free(&m);
    scene_free(&s);
}

/* Brightest channel at the screen center */
static int center_peak(const Framebuffer *fb) {
    uint32_t p = fb->pixels[(SCREEN_H / 2) * SCREEN_W + SCREEN_W / 2];
    int r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
    return r > g ? (r > b ? r : b) : (g > b ? g : b);
}

/* A lamp straight ahead shows as a glowing dot; one whose source is
 * drawn by something else (no_bulb) doesn't */
TEST(test_render_no_bulb) {
    Scene s;
    scene_init(&s);
    Camera cam = camera_create(vec3(0, 0, 0), 0, 0, 1.2f);
    Light lamp = {.position = vec3(0, 0, -3), .color = vec3(1, 1, 1),
                  .intensity = 1.0f, .radius = 5.0f};

    light_add(&s.lights, lamp);
    render_scene_lit(&frame_a, &cam, &s, 0, NULL);
    s.lights.items[0].no_bulb = true;
    render_scene_lit(&frame_b, &cam, &s, 0, NULL);

    ASSERT(center_peak(&frame_a) > 200);
    ASSERT(center_peak(&frame_b) < 100);
    scene_free(&s);
}

/* A sprite spanning many 16 px tiles is drawn in every one of them
 * (per-tile culling keeps it wherever its screen rect reaches) */
TEST(test_render_sprite_across_tiles) {
    Scene s;
    scene_init(&s);
    Camera cam = camera_create(vec3(0, 1, 0), 0, 0, 1.2f);
    render_scene_lit(&frame_a, &cam, &s, 0, NULL);   /* sky only */

    Sprite wide = {.base = vec3(0, 0, -3), .width = 3.0f, .height = 2.0f,
                   .material_id = -1, .frame_count = 1};
    s.sprites = &wide;
    s.sprite_count = 1;
    render_scene_lit(&frame_b, &cam, &s, 0, NULL);

    int left, top, right, bottom;
    float dist;
    ASSERT(camera_project(&cam, vec3(-1.4f, 0.2f, -3), &left, &bottom, &dist));
    ASSERT(camera_project(&cam, vec3(1.4f, 1.8f, -3), &right, &top, &dist));
    for (int y = top; y <= bottom; y += 4) {
        for (int x = left; x <= right; x += 4) {
            int i = y * SCREEN_W + x;
            ASSERT(frame_a.pixels[i] != frame_b.pixels[i]);
        }
    }
    s.sprites = NULL;
    s.sprite_count = 0;
    scene_free(&s);
}

int main(void) {
    TEST_SUITE("color");
    RUN_TEST(test_color_rgb);
    RUN_TEST(test_color_rgb_black);
    RUN_TEST(test_color_rgb_white);

    TEST_SUITE("framebuffer");
    RUN_TEST(test_framebuffer_set_and_read);
    RUN_TEST(test_framebuffer_clear);
    RUN_TEST(test_framebuffer_bounds_check);

    TEST_SUITE("render");
    RUN_TEST(test_render_affine_warp);
    RUN_TEST(test_render_no_bulb);
    RUN_TEST(test_render_sprite_across_tiles);
    TEST_REPORT();
}
