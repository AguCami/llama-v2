/*
 * Rasterizador por software con z-buffer (1/w).
 *
 * Pipeline: modelo -> mundo -> clip -> recorte contra plano cercano ->
 *           division perspectiva -> pantalla -> edge functions.
 *
 * Sombreado: las caras marcadas con G3D_TRI_SMOOTH interpolan la intensidad
 * entre sus vertices (gouraud); el resto usa la normal de la cara (plano).
 * Para no recalcular colores por pixel, cada triangulo suave arma una tabla de
 * 32 tonos de su color base y el bucle interno solo indexa.
 */
#include "g3d.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SHADE_LEVELS 32
#define SHADE_STEP   0.05f          /* tono k = color * (k * 0.05) */
#define SHADE_SCALE  (1.f / SHADE_STEP)

typedef struct { float x, y, z, w; } g3d_v4;
typedef struct { float sx, sy, invw, sh; } g3d_sv;

/* Buffers temporales de la malla en curso. */
static g3d_v4 *s_clip;
static g3d_v3 *s_world;
static float  *s_shade;
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
    float *sh = (float *)realloc(s_shade, sizeof(float) * (size_t)cap);
    if (!sh) return false;
    s_shade = sh;
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

/* Degradado con tramado ordenado 4x4: en RGB565 un degradado liso se ve a
 * bandas, y el tramado las disimula sin costo apreciable. */
void g3d_sky_gradient(g3d_target *t, g3d_color top, g3d_color bottom, int y0, int y1)
{
    static const int BAYER[4][4] = {
        {  0,  8,  2, 10 },
        { 12,  4, 14,  6 },
        {  3, 11,  1,  9 },
        { 15,  7, 13,  5 },
    };
    if (y0 < 0) y0 = 0;
    if (y1 > t->h) y1 = t->h;
    const int span = (y1 - y0) > 1 ? (y1 - y0 - 1) : 1;

    const int tr = (top >> 11) & 0x1F, tg = (top >> 5) & 0x3F, tb = top & 0x1F;
    const int br = (bottom >> 11) & 0x1F, bg = (bottom >> 5) & 0x3F, bb = bottom & 0x1F;

    for (int y = y0; y < y1; y++) {
        const int f = ((y - y0) * 256) / span;      /* 0..256 */
        const int r16 = tr * 16 + ((br - tr) * f * 16) / 256;
        const int g16 = tg * 16 + ((bg - tg) * f * 16) / 256;
        const int b16 = tb * 16 + ((bb - tb) * f * 16) / 256;
        g3d_color *row = t->color + (size_t)y * t->w;
        const int *bay = BAYER[y & 3];
        for (int x = 0; x < t->w; x++) {
            const int d = bay[x & 3];
            int r = (r16 + d) >> 4, g = (g16 + d) >> 4, b = (b16 + d) >> 4;
            if (r > 31) r = 31;
            if (g > 63) g = 63;
            if (b > 31) b = 31;
            row[x] = (g3d_color)((r << 11) | (g << 5) | b);
        }
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

typedef struct { g3d_v4 c; float sh; } g3d_cv;

static inline g3d_cv cv_lerp(g3d_cv a, g3d_cv b, float t)
{
    g3d_cv r;
    r.c.x = a.c.x + (b.c.x - a.c.x) * t;
    r.c.y = a.c.y + (b.c.y - a.c.y) * t;
    r.c.z = a.c.z + (b.c.z - a.c.z) * t;
    r.c.w = a.c.w + (b.c.w - a.c.w) * t;
    r.sh  = a.sh  + (b.sh  - a.sh)  * t;
    return r;
}

/* Recorte contra el plano cercano (z + w > 0). */
static int clip_near(const g3d_cv *in, int n, g3d_cv *out)
{
    int m = 0;
    for (int i = 0; i < n; i++) {
        g3d_cv a = in[i];
        g3d_cv b = in[(i + 1) % n];
        float da = a.c.z + a.c.w;
        float db = b.c.z + b.c.w;
        bool ina = da > 1e-5f;
        bool inb = db > 1e-5f;
        if (ina) out[m++] = a;
        if (ina != inb) out[m++] = cv_lerp(a, b, da / (da - db));
        if (m >= 6) break;
    }
    return m;
}

/* --------------------------------------------------------------- rasterizado */

typedef struct {
    float area, inv_area;
    int   minx, maxx, miny, maxy;
    float e0_dx, e0_dy, e1_dx, e1_dy, e2_dx, e2_dy;
    float e0_row, e1_row, e2_row;
} tri_setup;

static bool setup_tri(const g3d_target *t, g3d_sv a, g3d_sv b, g3d_sv c, tri_setup *s)
{
    /* La proyeccion invierte Y, asi que una cara frontal queda con determinante
     * negativo: trabajamos con el signo invertido. */
    s->area = (c.sx - a.sx) * (b.sy - a.sy) - (b.sx - a.sx) * (c.sy - a.sy);
    if (s->area <= 0.f) return false;

    s->minx = (int)floorf(fminf(a.sx, fminf(b.sx, c.sx)));
    s->maxx = (int)ceilf (fmaxf(a.sx, fmaxf(b.sx, c.sx)));
    s->miny = (int)floorf(fminf(a.sy, fminf(b.sy, c.sy)));
    s->maxy = (int)ceilf (fmaxf(a.sy, fmaxf(b.sy, c.sy)));
    if (s->minx < 0) s->minx = 0;
    if (s->miny < 0) s->miny = 0;
    if (s->maxx > t->w - 1) s->maxx = t->w - 1;
    if (s->maxy > t->h - 1) s->maxy = t->h - 1;
    if (s->minx > s->maxx || s->miny > s->maxy) return false;

    s->inv_area = 1.f / s->area;
    s->e0_dx = (c.sy - b.sy); s->e0_dy = -(c.sx - b.sx);
    s->e1_dx = (a.sy - c.sy); s->e1_dy = -(a.sx - c.sx);
    s->e2_dx = (b.sy - a.sy); s->e2_dy = -(b.sx - a.sx);

    const float px = (float)s->minx + 0.5f, py = (float)s->miny + 0.5f;
    s->e0_row = (c.sy - b.sy) * (px - b.sx) - (c.sx - b.sx) * (py - b.sy);
    s->e1_row = (a.sy - c.sy) * (px - c.sx) - (a.sx - c.sx) * (py - c.sy);
    s->e2_row = (b.sy - a.sy) * (px - a.sx) - (b.sx - a.sx) * (py - a.sy);
    return true;
}

static void raster_flat(g3d_target *t, g3d_sv a, g3d_sv b, g3d_sv c, g3d_color col)
{
    tri_setup s;
    if (!setup_tri(t, a, b, c, &s)) return;

    const float w_dx = (s.e0_dx * a.invw + s.e1_dx * b.invw + s.e2_dx * c.invw) * s.inv_area;
    const float w_dy = (s.e0_dy * a.invw + s.e1_dy * b.invw + s.e2_dy * c.invw) * s.inv_area;
    float w_row = (s.e0_row * a.invw + s.e1_row * b.invw + s.e2_row * c.invw) * s.inv_area;

    for (int y = s.miny; y <= s.maxy; y++) {
        float e0 = s.e0_row, e1 = s.e1_row, e2 = s.e2_row, iw = w_row;
        g3d_color *crow = t->color + (size_t)y * t->w;
        float *drow = t->depth + (size_t)y * t->w;
        for (int x = s.minx; x <= s.maxx; x++) {
            if (e0 >= 0.f && e1 >= 0.f && e2 >= 0.f && iw > drow[x]) {
                drow[x] = iw;
                crow[x] = col;
            }
            e0 += s.e0_dx; e1 += s.e1_dx; e2 += s.e2_dx; iw += w_dx;
        }
        s.e0_row += s.e0_dy; s.e1_row += s.e1_dy; s.e2_row += s.e2_dy;
        w_row += w_dy;
    }
}

static void raster_smooth(g3d_target *t, g3d_sv a, g3d_sv b, g3d_sv c,
                          const g3d_color *lut)
{
    tri_setup s;
    if (!setup_tri(t, a, b, c, &s)) return;

    const float w_dx = (s.e0_dx * a.invw + s.e1_dx * b.invw + s.e2_dx * c.invw) * s.inv_area;
    const float w_dy = (s.e0_dy * a.invw + s.e1_dy * b.invw + s.e2_dy * c.invw) * s.inv_area;
    float w_row = (s.e0_row * a.invw + s.e1_row * b.invw + s.e2_row * c.invw) * s.inv_area;

    const float s_dx = (s.e0_dx * a.sh + s.e1_dx * b.sh + s.e2_dx * c.sh) * s.inv_area;
    const float s_dy = (s.e0_dy * a.sh + s.e1_dy * b.sh + s.e2_dy * c.sh) * s.inv_area;
    float s_row = (s.e0_row * a.sh + s.e1_row * b.sh + s.e2_row * c.sh) * s.inv_area;

    for (int y = s.miny; y <= s.maxy; y++) {
        float e0 = s.e0_row, e1 = s.e1_row, e2 = s.e2_row;
        float iw = w_row, sh = s_row;
        g3d_color *crow = t->color + (size_t)y * t->w;
        float *drow = t->depth + (size_t)y * t->w;
        for (int x = s.minx; x <= s.maxx; x++) {
            if (e0 >= 0.f && e1 >= 0.f && e2 >= 0.f && iw > drow[x]) {
                int k = (int)sh;
                if (k < 0) k = 0;
                else if (k >= SHADE_LEVELS) k = SHADE_LEVELS - 1;
                drow[x] = iw;
                crow[x] = lut[k];
            }
            e0 += s.e0_dx; e1 += s.e1_dx; e2 += s.e2_dx;
            iw += w_dx; sh += s_dx;
        }
        s.e0_row += s.e0_dy; s.e1_row += s.e1_dy; s.e2_row += s.e2_dy;
        w_row += w_dy; s_row += s_dy;
    }
}

static inline g3d_sv to_screen(const g3d_target *t, g3d_cv v)
{
    g3d_sv s;
    float invw = 1.f / v.c.w;
    s.sx = (v.c.x * invw * 0.5f + 0.5f) * (float)t->w;
    s.sy = (0.5f - v.c.y * invw * 0.5f) * (float)t->h;
    s.invw = invw;
    s.sh = v.sh;
    return s;
}

/* ------------------------------------------------------------------ dibujo */

static inline float shade_of(const g3d_light *L, g3d_v3 n, g3d_v3 to_eye)
{
    float lambert = g3d_v3_dot(n, L->light_dir);
    if (lambert < 0.f) lambert = 0.f;
    /* Ambiente hemisferico: lo que mira al cielo recibe mas luz que lo que
     * mira al piso. Es un solo producto extra y le da mucho volumen. */
    float sky = 0.5f + 0.5f * n.y;
    float s = L->ambient * (0.70f + 0.46f * sky) + L->diffuse * lambert;
    if (L->rim > 0.f) {
        float rim = 1.f - g3d_v3_dot(n, to_eye);
        if (rim > 0.f) s += L->rim * rim * rim;
    }
    return s;
}

void g3d_draw_mesh(g3d_target *t, const g3d_ctx *ctx, const g3d_mesh *m,
                   const g3d_mat4 *model, float tint)
{
    if (!m->nt || !scratch_reserve(m->nv)) return;

    const g3d_light *L = &ctx->light;
    const bool fog = (L->fog_end > L->fog_start);

    /* Los triangulos de una misma pieza comparten color: reusamos la tabla. */
    g3d_color lut[SHADE_LEVELS];
    g3d_color lut_key = 0;
    bool      lut_valid = false;

    g3d_mat4 mvp = g3d_mat4_mul(ctx->view_proj, *model);
    for (int i = 0; i < m->nv; i++) {
        s_world[i] = g3d_mat4_point(model, m->v[i]);
        s_clip[i]  = clip_xform(&mvp, m->v[i]);
        s_shade[i] = -1.f;   /* perezoso: se calcula si hace falta */
    }

    for (int i = 0; i < m->nt; i++) {
        const g3d_tri *tri = &m->t[i];
        const g3d_v3 p0 = s_world[tri->a], p1 = s_world[tri->b], p2 = s_world[tri->c];
        g3d_v3 fn = g3d_v3_cross(g3d_v3_sub(p1, p0), g3d_v3_sub(p2, p0));
        float nl = g3d_v3_len(fn);
        if (nl < 1e-9f) continue;
        fn = g3d_v3_mul(fn, 1.f / nl);

        g3d_v3 to_eye_v = g3d_v3_sub(ctx->eye, p0);
        float dist = g3d_v3_len(to_eye_v);
        g3d_v3 to_eye = (dist > 1e-6f) ? g3d_v3_mul(to_eye_v, 1.f / dist) : g3d_v(0.f, 0.f, 1.f);
        if (g3d_v3_dot(fn, to_eye) <= 0.f) continue;   /* cara trasera */

        /* Niebla por cara: acerca el color al horizonte segun la distancia. */
        g3d_color base = tri->color;
        if (fog) {
            float f = (dist - L->fog_start) / (L->fog_end - L->fog_start);
            if (f > 0.f) base = g3d_color_lerp(base, L->fog_color, f > 1.f ? 1.f : f);
        }

        g3d_cv poly[8], tmp[8];
        const bool smooth = (tri->flags & G3D_TRI_SMOOTH) != 0;
        g3d_color flat_col = 0;

        if (smooth) {
            const uint16_t idx[3] = { tri->a, tri->b, tri->c };
            for (int k = 0; k < 3; k++) {
                if (s_shade[idx[k]] < 0.f) {
                    g3d_v3 n = g3d_v3_norm(g3d_mat4_dir(model, m->n[idx[k]]));
                    g3d_v3 te = g3d_v3_norm(g3d_v3_sub(ctx->eye, s_world[idx[k]]));
                    s_shade[idx[k]] = shade_of(L, n, te);
                }
            }
            if (!lut_valid || lut_key != base) {
                for (int k = 0; k < SHADE_LEVELS; k++) {
                    lut[k] = g3d_color_shade(base, (float)k * SHADE_STEP * tint);
                }
                lut_key = base;
                lut_valid = true;
            }
            tmp[0].sh = s_shade[tri->a] * SHADE_SCALE;
            tmp[1].sh = s_shade[tri->b] * SHADE_SCALE;
            tmp[2].sh = s_shade[tri->c] * SHADE_SCALE;
        } else {
            float s = shade_of(L, fn, to_eye) * tint;
            if (s > 1.6f) s = 1.6f;
            flat_col = g3d_color_shade(base, s);
            tmp[0].sh = tmp[1].sh = tmp[2].sh = 0.f;
        }

        tmp[0].c = s_clip[tri->a];
        tmp[1].c = s_clip[tri->b];
        tmp[2].c = s_clip[tri->c];

        int n_poly;
        if (tmp[0].c.z + tmp[0].c.w > 1e-5f && tmp[1].c.z + tmp[1].c.w > 1e-5f &&
            tmp[2].c.z + tmp[2].c.w > 1e-5f) {
            poly[0] = tmp[0]; poly[1] = tmp[1]; poly[2] = tmp[2];
            n_poly = 3;
        } else {
            n_poly = clip_near(tmp, 3, poly);
            if (n_poly < 3) continue;
        }

        g3d_sv sv[8];
        for (int k = 0; k < n_poly; k++) sv[k] = to_screen(t, poly[k]);
        for (int k = 1; k + 1 < n_poly; k++) {
            if (smooth) raster_smooth(t, sv[0], sv[k], sv[k + 1], lut);
            else        raster_flat(t, sv[0], sv[k], sv[k + 1], flat_col);
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
