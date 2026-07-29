#include "g3d.h"
#include <math.h>
#include <string.h>

g3d_color g3d_color_shade(g3d_color c, float f)
{
    if (f < 0.f) f = 0.f;
    int r = (int)(((c >> 11) & 0x1F) * f);
    int g = (int)(((c >> 5) & 0x3F) * f);
    int b = (int)((c & 0x1F) * f);
    if (r > 31) r = 31;
    if (g > 63) g = 63;
    if (b > 31) b = 31;
    return (g3d_color)((r << 11) | (g << 5) | b);
}

g3d_color g3d_color_lerp(g3d_color a, g3d_color b, float t)
{
    if (t <= 0.f) return a;
    if (t >= 1.f) return b;
    int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    int r = ar + (int)((br - ar) * t);
    int g = ag + (int)((bg - ag) * t);
    int bl = ab + (int)((bb - ab) * t);
    return (g3d_color)((r << 11) | (g << 5) | bl);
}

g3d_v3 g3d_v3_add(g3d_v3 a, g3d_v3 b) { return g3d_v(a.x + b.x, a.y + b.y, a.z + b.z); }
g3d_v3 g3d_v3_sub(g3d_v3 a, g3d_v3 b) { return g3d_v(a.x - b.x, a.y - b.y, a.z - b.z); }
g3d_v3 g3d_v3_mul(g3d_v3 a, float s)  { return g3d_v(a.x * s, a.y * s, a.z * s); }
float  g3d_v3_dot(g3d_v3 a, g3d_v3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

g3d_v3 g3d_v3_cross(g3d_v3 a, g3d_v3 b)
{
    return g3d_v(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

float g3d_v3_len(g3d_v3 a) { return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z); }

g3d_v3 g3d_v3_norm(g3d_v3 a)
{
    float l = g3d_v3_len(a);
    if (l < 1e-6f) return g3d_v(0.f, 1.f, 0.f);
    return g3d_v3_mul(a, 1.f / l);
}

g3d_v3 g3d_v3_lerp(g3d_v3 a, g3d_v3 b, float t)
{
    return g3d_v(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

g3d_mat4 g3d_mat4_identity(void)
{
    g3d_mat4 r;
    memset(r.m, 0, sizeof(r.m));
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f;
    return r;
}

g3d_mat4 g3d_mat4_mul(g3d_mat4 a, g3d_mat4 b)
{
    g3d_mat4 r;
    for (int i = 0; i < 4; i++) {
        const float a0 = a.m[i * 4 + 0], a1 = a.m[i * 4 + 1];
        const float a2 = a.m[i * 4 + 2], a3 = a.m[i * 4 + 3];
        for (int j = 0; j < 4; j++) {
            r.m[i * 4 + j] = a0 * b.m[0 * 4 + j] + a1 * b.m[1 * 4 + j] +
                             a2 * b.m[2 * 4 + j] + a3 * b.m[3 * 4 + j];
        }
    }
    return r;
}

g3d_mat4 g3d_mat4_translate(float x, float y, float z)
{
    g3d_mat4 r = g3d_mat4_identity();
    r.m[3] = x; r.m[7] = y; r.m[11] = z;
    return r;
}

g3d_mat4 g3d_mat4_scale(float x, float y, float z)
{
    g3d_mat4 r = g3d_mat4_identity();
    r.m[0] = x; r.m[5] = y; r.m[10] = z;
    return r;
}

g3d_mat4 g3d_mat4_rot_x(float a)
{
    g3d_mat4 r = g3d_mat4_identity();
    float c = cosf(a), s = sinf(a);
    r.m[5] = c; r.m[6] = -s;
    r.m[9] = s; r.m[10] = c;
    return r;
}

g3d_mat4 g3d_mat4_rot_y(float a)
{
    g3d_mat4 r = g3d_mat4_identity();
    float c = cosf(a), s = sinf(a);
    r.m[0] = c;  r.m[2] = s;
    r.m[8] = -s; r.m[10] = c;
    return r;
}

g3d_mat4 g3d_mat4_rot_z(float a)
{
    g3d_mat4 r = g3d_mat4_identity();
    float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[1] = -s;
    r.m[4] = s; r.m[5] = c;
    return r;
}

g3d_mat4 g3d_mat4_perspective(float fovy_rad, float aspect, float znear, float zfar)
{
    g3d_mat4 r;
    memset(r.m, 0, sizeof(r.m));
    float f = 1.f / tanf(fovy_rad * 0.5f);
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = (2.f * zfar * znear) / (znear - zfar);
    r.m[14] = -1.f;
    return r;
}

g3d_mat4 g3d_mat4_look_at(g3d_v3 eye, g3d_v3 center, g3d_v3 up)
{
    g3d_v3 f = g3d_v3_norm(g3d_v3_sub(center, eye));
    g3d_v3 s = g3d_v3_norm(g3d_v3_cross(f, up));
    g3d_v3 u = g3d_v3_cross(s, f);
    g3d_mat4 r = g3d_mat4_identity();
    r.m[0] = s.x;  r.m[1] = s.y;  r.m[2] = s.z;  r.m[3]  = -g3d_v3_dot(s, eye);
    r.m[4] = u.x;  r.m[5] = u.y;  r.m[6] = u.z;  r.m[7]  = -g3d_v3_dot(u, eye);
    r.m[8] = -f.x; r.m[9] = -f.y; r.m[10] = -f.z; r.m[11] = g3d_v3_dot(f, eye);
    return r;
}

g3d_v3 g3d_mat4_point(const g3d_mat4 *m, g3d_v3 p)
{
    return g3d_v(m->m[0] * p.x + m->m[1] * p.y + m->m[2] * p.z + m->m[3],
                 m->m[4] * p.x + m->m[5] * p.y + m->m[6] * p.z + m->m[7],
                 m->m[8] * p.x + m->m[9] * p.y + m->m[10] * p.z + m->m[11]);
}

g3d_v3 g3d_mat4_dir(const g3d_mat4 *m, g3d_v3 d)
{
    return g3d_v(m->m[0] * d.x + m->m[1] * d.y + m->m[2] * d.z,
                 m->m[4] * d.x + m->m[5] * d.y + m->m[6] * d.z,
                 m->m[8] * d.x + m->m[9] * d.y + m->m[10] * d.z);
}
