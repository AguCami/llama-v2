/*
 * Rasterizador por software con z-buffer (1/w) y sombreado plano por cara.
 *
 * Pipeline: modelo -> mundo -> clip -> recorte contra plano cercano ->
 *           division perspectiva -> pantalla -> edge functions.
 */
#include "g3d.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float x, y, z, w; } g3d_v4;
typedef struct { float sx, sy, invw; } g3d_sv;

/* Buffer temporal de vertices transformados (una malla por vez). */
static g3d_v4 *s_clip;
static g3d_v3 *s_world;
static int     s_cap;

static bool scratch_reserve(int n)
{
    if (n <= s_cap) return true;
    int cap = n < 64 ? 64 : n;
    g3d_v4 *c = (g3d_v4 *)realloc(s_clip, sizeof(g3d_v4) * (size_t)cap);
    if (!c) return false;
    s_clip = c;
    g3d_v3 *w = (g3d_v3 *)realloc(s_world, sizeof(g3d_v3) * (size_t)cap);
    if (!w) return false;
    s_world = w;
    s_cap = cap;
    return true;
}

void g3d_clear_color(g3d_target *t, g3d_color c)
{
    int n = t->w * t->h;
    for (int i = 0; i < n; i++) t->color[i] = c;
}

void g3d_clear_depth(g3d_target *t)
{
    memset(t->depth, 0, sizeof(float) * (size_t)(t->w * t->h));
}

void g3d_sky_gradient(g3d_target *t, g3d_color top, g3d_color bottom, int y0, int y1)
{
    if (y0 < 0) y0 = 0;
    if (y1 > t->h) y1 = t->h;
    int span = (y1 - y0) > 1 ? (y1 - y0 - 1) : 1;
    for (int y = y0; y < y1; y++) {
        g3d_color c = g3d_color_lerp(top, bottom, (float)(y - y0) / (float)span);
        g3d_color *row = t->color + (size_t)y * t->w;
        for (int x = 0; x < t->w; x++) row[x] = c;
    }
}

void g3d_ctx_camera(g3d_ctx *ctx, g3d_v3 eye, g3d_v3 target, float fovy_rad,
                    float aspect, float znear, float zfar)
{
    ctx->eye    = eye;
    ctx->proj_f = 1.f / tanf(fovy_rad * 0.5f);
    ctx->view   = g3d_mat4_look_at(eye, target, g3d_v(0.f, 1.f, 0.f));
    ctx->view_proj = g3d_mat4_mul(g3d_mat4_perspective(fovy_rad, aspect, znear, zfar), ctx->view);
}

static inline g3d_v4 clip_xform(const g3d_mat4 *m, g3d_v3 p)
{
    g3d_v4 r;
    r.x = m->m[0]  * p.x + m->m[1]  * p.y + m->m[2]  * p.z + m->m[3];
    r.y = m->m[4]  * p.x + m->m[5]  * p.y + m->m[6]  * p.z + m->m[7];
    r.z = m->m[8]  * p.x + m->m[9]  * p.y + m->m[10] * p.z + m->m[11];
    r.w = m->m[12] * p.x + m->m[13] * p.y + m->m[14] * p.z + m->m[15];
    return r;
}

static inline g3d_v4 v4_lerp(g3d_v4 a, g3d_v4 b, float t)
{
    g3d_v4 r;
    r.x = a.x + (b.x - a.x) * t;
    r.y = a.y + (b.y - a.y) * t;
    r.z = a.z + (b.z - a.z) * t;
    r.w = a.w + (b.w - a.w) * t;
    return r;
}

/* Recorte contra el plano cercano (z + w > 0). Devuelve la cantidad de vertices. */
static int clip_near(const g3d_v4 *in, int n, g3d_v4 *out)
{
    int m = 0;
    for (int i = 0; i < n; i++) {
        g3d_v4 a = in[i];
        g3d_v4 b = in[(i + 1) % n];
        float da = a.z + a.w;
        float db = b.z + b.w;
        bool ina = da > 1e-5f;
        bool inb = db > 1e-5f;
        if (ina) out[m++] = a;
        if (ina != inb) {
            float t = da / (da - db);
            out[m++] = v4_lerp(a, b, t);
        }
        if (m >= 6) break;
    }
    return m;
}

static void raster_tri(g3d_target *t, g3d_sv a, g3d_sv b, g3d_sv c, g3d_color col)
{
    /* La proyeccion invierte el eje Y, asi que una cara frontal (antihoraria en
     * el mundo) queda antihoraria en pantalla con Y hacia abajo: area negativa
     * con el determinante clasico. Trabajamos con el signo invertido. */
    float area = (c.sx - a.sx) * (b.sy - a.sy) - (b.sx - a.sx) * (c.sy - a.sy);
    if (area <= 0.f) return; /* cara trasera o degenerada */

    int minx = (int)floorf(fminf(a.sx, fminf(b.sx, c.sx)));
    int maxx = (int)ceilf (fmaxf(a.sx, fmaxf(b.sx, c.sx)));
    int miny = (int)floorf(fminf(a.sy, fminf(b.sy, c.sy)));
    int maxy = (int)ceilf (fmaxf(a.sy, fmaxf(b.sy, c.sy)));
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx > t->w - 1) maxx = t->w - 1;
    if (maxy > t->h - 1) maxy = t->h - 1;
    if (minx > maxx || miny > maxy) return;

    const float inv_area = 1.f / area;

    /* Edge functions con el mismo signo invertido: dentro = las tres >= 0
     * y e0 + e1 + e2 = area. e0 pesa al vertice a, e1 a b, e2 a c. */
    const float e0_dx = (c.sy - b.sy), e0_dy = -(c.sx - b.sx); /* arista c->b */
    const float e1_dx = (a.sy - c.sy), e1_dy = -(a.sx - c.sx); /* arista a->c */
    const float e2_dx = (b.sy - a.sy), e2_dy = -(b.sx - a.sx); /* arista b->a */

    float px = (float)minx + 0.5f, py = (float)miny + 0.5f;
    float e0_row = (c.sy - b.sy) * (px - b.sx) - (c.sx - b.sx) * (py - b.sy);
    float e1_row = (a.sy - c.sy) * (px - c.sx) - (a.sx - c.sx) * (py - c.sy);
    float e2_row = (b.sy - a.sy) * (px - a.sx) - (b.sx - a.sx) * (py - a.sy);

    for (int y = miny; y <= maxy; y++) {
        float e0 = e0_row, e1 = e1_row, e2 = e2_row;
        size_t base = (size_t)y * t->w;
        for (int x = minx; x <= maxx; x++) {
            if (e0 >= 0.f && e1 >= 0.f && e2 >= 0.f) {
                float w0 = e0 * inv_area, w1 = e1 * inv_area, w2 = e2 * inv_area;
                float invw = w0 * a.invw + w1 * b.invw + w2 * c.invw;
                size_t idx = base + (size_t)x;
                if (invw > t->depth[idx]) {
                    t->depth[idx] = invw;
                    t->color[idx] = col;
                }
            }
            e0 += e0_dx; e1 += e1_dx; e2 += e2_dx;
        }
        e0_row += e0_dy; e1_row += e1_dy; e2_row += e2_dy;
    }
}

static inline g3d_sv to_screen(const g3d_target *t, g3d_v4 v)
{
    g3d_sv s;
    float invw = 1.f / v.w;
    s.sx = (v.x * invw * 0.5f + 0.5f) * (float)t->w;
    s.sy = (0.5f - v.y * invw * 0.5f) * (float)t->h;
    s.invw = invw;
    return s;
}

void g3d_draw_mesh(g3d_target *t, const g3d_ctx *ctx, const g3d_mesh *m,
                   const g3d_mat4 *model, float tint)
{
    if (!m->nt || !scratch_reserve(m->nv)) return;

    g3d_mat4 mvp = g3d_mat4_mul(ctx->view_proj, *model);
    for (int i = 0; i < m->nv; i++) {
        s_world[i] = g3d_mat4_point(model, m->v[i]);
        s_clip[i]  = clip_xform(&mvp, m->v[i]);
    }

    const g3d_light *L = &ctx->light;
    for (int i = 0; i < m->nt; i++) {
        const g3d_tri *tri = &m->t[i];
        g3d_v3 p0 = s_world[tri->a], p1 = s_world[tri->b], p2 = s_world[tri->c];
        g3d_v3 n = g3d_v3_cross(g3d_v3_sub(p1, p0), g3d_v3_sub(p2, p0));
        float nl = g3d_v3_len(n);
        if (nl < 1e-9f) continue;
        n = g3d_v3_mul(n, 1.f / nl);

        /* Descarte temprano: caras que miran para el otro lado. */
        g3d_v3 to_eye = g3d_v3_norm(g3d_v3_sub(ctx->eye, p0));
        float facing = g3d_v3_dot(n, to_eye);
        if (facing <= 0.f) continue;

        float lambert = g3d_v3_dot(n, L->light_dir);
        if (lambert < 0.f) lambert = 0.f;
        float shade = L->ambient + L->diffuse * lambert;
        if (L->rim > 0.f) {
            float rim = 1.f - facing;
            shade += L->rim * rim * rim;
        }
        shade *= tint;
        if (shade > 1.6f) shade = 1.6f;
        g3d_color col = g3d_color_shade(tri->color, shade);

        g3d_v4 poly[8], tmp[8];
        tmp[0] = s_clip[tri->a];
        tmp[1] = s_clip[tri->b];
        tmp[2] = s_clip[tri->c];

        int n_poly;
        if (tmp[0].z + tmp[0].w > 1e-5f && tmp[1].z + tmp[1].w > 1e-5f &&
            tmp[2].z + tmp[2].w > 1e-5f) {
            poly[0] = tmp[0]; poly[1] = tmp[1]; poly[2] = tmp[2];
            n_poly = 3;
        } else {
            n_poly = clip_near(tmp, 3, poly);
            if (n_poly < 3) continue;
        }

        g3d_sv sv[8];
        for (int k = 0; k < n_poly; k++) sv[k] = to_screen(t, poly[k]);
        for (int k = 1; k + 1 < n_poly; k++) {
            raster_tri(t, sv[0], sv[k], sv[k + 1], col);
        }
        ((g3d_ctx *)ctx)->tris_drawn++;
    }
}

bool g3d_project(const g3d_target *t, const g3d_ctx *ctx, g3d_v3 world,
                 float *sx, float *sy, float *scale)
{
    g3d_v4 c = clip_xform(&ctx->view_proj, world);
    if (c.w < 1e-4f) return false;
    float invw = 1.f / c.w;
    if (sx) *sx = (c.x * invw * 0.5f + 0.5f) * (float)t->w;
    if (sy) *sy = (0.5f - c.y * invw * 0.5f) * (float)t->h;
    if (scale) *scale = invw * (float)t->h * 0.5f * ctx->proj_f;
    return true;
}
