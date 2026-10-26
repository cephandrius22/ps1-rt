/*
 * ps1_effects.h — PlayStation 1 visual effect emulation.
 *
 * Vertex jitter: snaps vertex positions to a coarse grid (1/128 unit),
 *   simulating the PS1's fixed-point vertex precision.
 * Affine texture mapping: linear UV interpolation without perspective
 *   correction, producing the classic PS1 texture warping.
 * Dithering: 4x4 Bayer matrix ordered dithering with quantization to
 *   5 bits per channel (15-bit color), matching PS1 VRAM format.
 */
#ifndef PS1_EFFECTS_H
#define PS1_EFFECTS_H

#include "math3d.h"
#include "mesh.h"
#include <stdint.h>

#define PS1_VERTEX_SNAP 128.0f  // Grid resolution for vertex snapping

// Snap a vertex position to a coarse grid (simulates PS1 fixed-point precision)
Vec3 ps1_snap_vertex(Vec3 v);

// Apply vertex jitter to a triangle (returns jittered copy)
Triangle ps1_jitter_triangle(const Triangle *tri);

// Affine (non-perspective-correct) UV interpolation
Vec2 ps1_affine_uv(const Triangle *tri, float u, float v);

// Ordered dithering (4x4 Bayer matrix)
uint32_t ps1_dither_color(uint8_t r, uint8_t g, uint8_t b, int px, int py);

#endif
