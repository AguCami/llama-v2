/*
 * Piso texturizado al estilo "Mode 7" (ver g3d_ground.h para la idea).
 *
 * La cuenta: un rayo por el pixel (px, py) sale en direccion
 *     d = f + xk*s + yk*u
 * con f/s/u los ejes de la camara y xk/yk las coordenadas del pixel en el
 * plano de proyeccion. Como la camara no balancea, s es horizontal (s.y = 0),
 * asi que d.y no depende de la columna: el parametro t = -ojo.y / d.y con el
 * que el rayo pincha el plano y = 0 es el mismo para toda la fila. El punto
 * P = ojo + t*d queda entonces afin en xk, y como los ejes son ortonormales la
 * profundidad de vista es w = t, tambien constante por fila.
 */
#include "g3d_ground.h"
#include <string.h>

#define GROUND_HEAD 20

static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

bool g3d_ground_open(g3d_ground_set *s, const void *blob, size_t len)
{
    memset(s, 0, sizeof(*s));
    if (!blob || len < GROUND_HEAD) return false;
    const uint8_t *p = (const uint8_t *)blob;
    if (memcmp(p, "LGND", 4) != 0) return false;
    if (rd16(p + 4) != 1) return false;

    s->blob  = p;
    s->len   = len;
    s->count = rd16(p + 6);
    s->size  = rd16(p + 8);
    uint16_t span_q8 = rd16(p + 10);      /* unidades de mundo en 8.8 */
    s->span  = (float)span_q8 / 256.f;
    s->table = p + GROUND_HEAD;

    if (s->count <= 0 || s->size <= 0 || s->span <= 0.f) return false;
    if ((s->size & (s->size - 1)) != 0) return false;   /* potencia de dos */
    s->shift = 0;
    while ((1 << s->shift) < s->size) s->shift++;
    if (len < GROUND_HEAD + (size_t)s->count * 4) return false;
    return true;
}

void g3d_ground_draw(g3d_target *t, const g3d_ctx *ctx, const g3d_ground_set *s,
                     int index, int y_top,
                     float shade, g3d_color tint_b, uint8_t tint_a)
{
    if (!s->blob) return;
    if (index < 0 || index >= s->count) index = 0;

    const uint32_t off = rd32(s->table + index * 4);
    const size_t need = (size_t)s->size * s->size * 2;
    if (off + need > s->len) return;
    const uint16_t *tex = (const uint16_t *)(const void *)(s->blob + off);

    /* Ejes de la camara, sacados de las filas de la matriz de vista:
     * fila 0 = s (derecha), fila 1 = u (arriba), fila 2 = -f (adelante). */
    const float *m = ctx->view.m;
    const g3d_v3 sv = g3d_v(m[0], m[1], m[2]);
    const g3d_v3 uv = g3d_v(m[4], m[5], m[6]);
    const g3d_v3 fv = g3d_v(-m[8], -m[9], -m[10]);
    const g3d_v3 eye = ctx->eye;
    if (eye.y <= 0.01f) return;                 /* la camara nunca baja al piso */

    const float tanfy = 1.f / ctx->proj_f;
    const float tanfx = tanfy * (float)t->w / (float)t->h;
    const float texel_per_unit = (float)s->size / s->span;
    const uint32_t mask = (uint32_t)s->size - 1;
    const int shift = s->shift;

    const g3d_light *L = &ctx->light;
    const float fog_num = 1.f / (L->fog_end - L->fog_start);
    const bool  amb_shade = (shade < 0.995f || shade > 1.005f);
    const float ta = tint_a / 255.f;

    if (y_top < 0) y_top = 0;

    for (int y = y_top; y < t->h; y++) {
        const float yk = (1.f - 2.f * ((float)y + 0.5f) / (float)t->h) * tanfy;
        const float dy = fv.y + yk * uv.y;
        if (dy >= -1e-4f) continue;             /* fila en o sobre el horizonte */
        const float dist = -eye.y / dy;         /* profundidad de toda la fila */

        /* Punto del plano bajo el primer pixel y paso por columna. */
        const float xk0 = (1.f / (float)t->w - 1.f) * tanfx;
        const float xstep = (2.f * tanfx / (float)t->w);
        float wx = eye.x + dist * (fv.x + yk * uv.x + xk0 * sv.x);
        float wz = eye.z + dist * (fv.z + yk * uv.z + xk0 * sv.z);
        const float sx_ = dist * xstep * sv.x;
        const float sz_ = dist * xstep * sv.z;

        /* Coordenadas de textura en punto fijo 16.16. */
        uint32_t tu = (uint32_t)(int32_t)(wx * texel_per_unit * 65536.f);
        uint32_t tv = (uint32_t)(int32_t)(wz * texel_per_unit * 65536.f);
        const uint32_t du = (uint32_t)(int32_t)(sx_ * texel_per_unit * 65536.f);
        const uint32_t dv = (uint32_t)(int32_t)(sz_ * texel_per_unit * 65536.f);

        /* Niebla y ambiente, constantes en la fila: una sola mezcla por pixel
         * como mucho (color destino y peso precalculados aca). */
        float ff = (dist - L->fog_start) * fog_num;
        if (ff < 0.f) ff = 0.f;
        if (ff > 1.f) ff = 1.f;
        const bool do_fog  = ff > 0.004f;
        const bool do_tint = tint_a > 0;

        const float invw = 1.f / dist;
        g3d_color *dst = t->color + (size_t)y * t->w;
        float     *dep = t->depth + (size_t)y * t->w;

        for (int x = 0; x < t->w; x++) {
            uint32_t ui = (tu >> 16) & mask;
            uint32_t vi = (tv >> 16) & mask;
            g3d_color c = tex[(vi << shift) | ui];
            if (amb_shade) c = g3d_color_shade(c, shade);
            if (do_fog)    c = g3d_color_lerp(c, L->fog_color, ff);
            if (do_tint)   c = g3d_color_lerp(c, tint_b, ta);
            dst[x] = c;
            dep[x] = invw;
            tu += du;
            tv += dv;
        }
    }
}
