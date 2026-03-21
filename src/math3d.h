/*
 * math3d.h — Inline 3D math library.
 *
 * Provides Vec3, Vec2, Mat4, and Ray types with common operations.
 * All functions are static inline for zero-overhead use in hot loops.
 */
#ifndef MATH3D_H
#define MATH3D_H

#include <math.h>

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float u, v;
} Vec2;

typedef struct {
    float m[4][4];
} Mat4;

typedef struct {
    Vec3 origin;
    Vec3 dir;
} Ray;

static inline Vec3 vec3(float x, float y, float z) {
    return (Vec3){x, y, z};
}

static inline Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline Vec3 vec3_mul(Vec3 a, float s) {
    return (Vec3){a.x * s, a.y * s, a.z * s};
}

static inline Vec3 vec3_neg(Vec3 a) {
    return (Vec3){-a.x, -a.y, -a.z};
}

static inline float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static inline float vec3_length(Vec3 a) {
    return sqrtf(vec3_dot(a, a));
}

static inline Vec3 vec3_normalize(Vec3 a) {
    float len = vec3_length(a);
    if (len < 1e-8f) return (Vec3){0, 0, 0};
    return vec3_mul(a, 1.0f / len);
}

static inline Vec3 vec3_lerp(Vec3 a, Vec3 b, float t) {
    return vec3_add(vec3_mul(a, 1.0f - t), vec3_mul(b, t));
}

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline Mat4 mat4_identity(void) {
    Mat4 m = {{{0}}};
    m.m[0][0] = m.m[1][1] = m.m[2][2] = m.m[3][3] = 1.0f;
    return m;
}

static inline Vec3 mat4_mul_vec3(Mat4 m, Vec3 v, float w) {
    return (Vec3){
        m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z + m.m[0][3] * w,
        m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z + m.m[1][3] * w,
        m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z + m.m[2][3] * w
    };
}

static inline Mat4 mat4_mul(Mat4 a, Mat4 b) {
    Mat4 r = {{{0}}};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}

static inline Mat4 mat4_rotation_y(float angle) {
    Mat4 m = mat4_identity();
    float c = cosf(angle), s = sinf(angle);
    m.m[0][0] = c;  m.m[0][2] = s;
    m.m[2][0] = -s; m.m[2][2] = c;
    return m;
}

static inline Mat4 mat4_rotation_x(float angle) {
    Mat4 m = mat4_identity();
    float c = cosf(angle), s = sinf(angle);
    m.m[1][1] = c;  m.m[1][2] = -s;
    m.m[2][1] = s;  m.m[2][2] = c;
    return m;
}

#endif
