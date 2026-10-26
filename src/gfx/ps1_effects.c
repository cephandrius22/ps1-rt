#include "ps1_effects.h"

Vec3 ps1_snap_vertex(Vec3 v) {
    return (Vec3){
        floorf(v.x * PS1_VERTEX_SNAP + 0.5f) / PS1_VERTEX_SNAP,
        floorf(v.y * PS1_VERTEX_SNAP + 0.5f) / PS1_VERTEX_SNAP,
        floorf(v.z * PS1_VERTEX_SNAP + 0.5f) / PS1_VERTEX_SNAP
    };
}

Triangle ps1_jitter_triangle(const Triangle *tri) {
    Triangle jittered = *tri;
    jittered.v0 = ps1_snap_vertex(tri->v0);
    jittered.v1 = ps1_snap_vertex(tri->v1);
    jittered.v2 = ps1_snap_vertex(tri->v2);
    return jittered;
}

Vec2 ps1_affine_uv(const Triangle *tri, float u, float v) {
    // Linear (affine) interpolation — no perspective correction
    // This produces the classic PS1 texture warping
    float w = 1.0f - u - v;
    return (Vec2){
        w * tri->uv0.u + u * tri->uv1.u + v * tri->uv2.u,
        w * tri->uv0.v + u * tri->uv1.v + v * tri->uv2.v
    };
}

// 4x4 Bayer dithering matrix (normalized to 0-15)
static const int bayer4x4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5}
};

uint32_t ps1_dither_color(uint8_t r, uint8_t g, uint8_t b, int px, int py) {
    float threshold = (bayer4x4[py & 3][px & 3] / 16.0f - 0.5f) * (255.0f / 8.0f);

    int ri = (int)r + (int)threshold;
    int gi = (int)g + (int)threshold;
    int bi = (int)b + (int)threshold;

    // Quantize to 5-bit per channel (PS1 = 15-bit color)
    ri = (ri < 0 ? 0 : (ri > 255 ? 255 : ri)) >> 3 << 3;
    gi = (gi < 0 ? 0 : (gi > 255 ? 255 : gi)) >> 3 << 3;
    bi = (bi < 0 ? 0 : (bi > 255 ? 255 : bi)) >> 3 << 3;

    return 0xFF000000 | ((uint32_t)ri << 16) | ((uint32_t)gi << 8) | (uint32_t)bi;
}
