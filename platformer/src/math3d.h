/*
 * math3d.h - minimal 3D math for the platformer.
 *
 * Header-only, C99, no dependencies beyond <math.h>.
 * Matrices are column-major (m[col][row]) to match the layout that
 * OpenGL's glLoadMatrixf / glMultMatrixf expect, so a Mat4 can be fed
 * straight to GL without transposing.
 */
#ifndef MATH3D_H
#define MATH3D_H

#include <math.h>

#ifndef M3_PI
#define M3_PI 3.14159265358979323846f
#endif

typedef struct { float x, y, z; }    Vec3;
typedef struct { float m[4][4]; }    Mat4; /* m[col][row] */

static inline float m3_deg2rad(float d) { return d * (M3_PI / 180.0f); }
static inline float m3_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float m3_lerpf(float a, float b, float t) { return a + (b - a) * t; }

/* ---- Vec3 ---- */
static inline Vec3 vec3(float x, float y, float z) { Vec3 v; v.x=x; v.y=y; v.z=z; return v; }
static inline Vec3 vec3_zero(void) { return vec3(0,0,0); }

static inline Vec3 vec3_add(Vec3 a, Vec3 b)  { return vec3(a.x+b.x, a.y+b.y, a.z+b.z); }
static inline Vec3 vec3_sub(Vec3 a, Vec3 b)  { return vec3(a.x-b.x, a.y-b.y, a.z-b.z); }
static inline Vec3 vec3_scale(Vec3 a, float s){ return vec3(a.x*s, a.y*s, a.z*s); }
static inline float vec3_dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

static inline Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return vec3(a.y*b.z - a.z*b.y,
                a.z*b.x - a.x*b.z,
                a.x*b.y - a.y*b.x);
}

static inline float vec3_len(Vec3 a) { return sqrtf(vec3_dot(a, a)); }

static inline Vec3 vec3_norm(Vec3 a) {
    float l = vec3_len(a);
    if (l > 1e-6f) return vec3_scale(a, 1.0f / l);
    return a;
}

static inline Vec3 vec3_lerp(Vec3 a, Vec3 b, float t) {
    return vec3(m3_lerpf(a.x,b.x,t), m3_lerpf(a.y,b.y,t), m3_lerpf(a.z,b.z,t));
}

/* ---- Mat4 (column-major) ---- */
static inline Mat4 mat4_identity(void) {
    Mat4 r;
    int c, rrow;
    for (c = 0; c < 4; ++c)
        for (rrow = 0; rrow < 4; ++rrow)
            r.m[c][rrow] = (c == rrow) ? 1.0f : 0.0f;
    return r;
}

static inline Mat4 mat4_mul(Mat4 a, Mat4 b) {
    /* result = a * b */
    Mat4 r;
    int c, rrow, k;
    for (c = 0; c < 4; ++c) {
        for (rrow = 0; rrow < 4; ++rrow) {
            float s = 0.0f;
            for (k = 0; k < 4; ++k)
                s += a.m[k][rrow] * b.m[c][k];
            r.m[c][rrow] = s;
        }
    }
    return r;
}

static inline Mat4 mat4_translate(Vec3 t) {
    Mat4 r = mat4_identity();
    r.m[3][0] = t.x;
    r.m[3][1] = t.y;
    r.m[3][2] = t.z;
    return r;
}

static inline Mat4 mat4_scale(Vec3 s) {
    Mat4 r = mat4_identity();
    r.m[0][0] = s.x;
    r.m[1][1] = s.y;
    r.m[2][2] = s.z;
    return r;
}

static inline Mat4 mat4_rotate_y(float radians) {
    Mat4 r = mat4_identity();
    float c = cosf(radians), s = sinf(radians);
    r.m[0][0] =  c; r.m[2][0] = s;
    r.m[0][2] = -s; r.m[2][2] = c;
    return r;
}

/* Right-handed perspective, maps z to [-1, 1] (GL convention). */
static inline Mat4 mat4_perspective(float fov_y_rad, float aspect, float znear, float zfar) {
    Mat4 r;
    int c, rrow;
    float f = 1.0f / tanf(fov_y_rad * 0.5f);
    for (c = 0; c < 4; ++c)
        for (rrow = 0; rrow < 4; ++rrow)
            r.m[c][rrow] = 0.0f;
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = (zfar + znear) / (znear - zfar);
    r.m[2][3] = -1.0f;
    r.m[3][2] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
}

/* Right-handed look-at view matrix. */
static inline Mat4 mat4_lookat(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = vec3_norm(vec3_sub(center, eye));
    Vec3 s = vec3_norm(vec3_cross(f, up));
    Vec3 u = vec3_cross(s, f);
    Mat4 r = mat4_identity();
    r.m[0][0] = s.x; r.m[1][0] = s.y; r.m[2][0] = s.z;
    r.m[0][1] = u.x; r.m[1][1] = u.y; r.m[2][1] = u.z;
    r.m[0][2] = -f.x; r.m[1][2] = -f.y; r.m[2][2] = -f.z;
    r.m[3][0] = -vec3_dot(s, eye);
    r.m[3][1] = -vec3_dot(u, eye);
    r.m[3][2] =  vec3_dot(f, eye);
    return r;
}

#endif /* MATH3D_H */
