/*
 * Copiado de sprites con escala (vecino mas cercano, punto fijo 16.16),
 * alfa de 4 bits y prueba de profundidad contra el z-buffer del corral.
 */
#include "g3d_sprite.h"
#include <string.h>

#define HEAD_SIZE  24
#define ENTRY_SIZE 12

static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline int16_t  rds16(const uint8_t *p) { return (int16_t)rd16(p); }
static inline uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

bool g3d_sprite_open(g3d_sprite_set *s, const void *blob, size_t len)
{
    memset(s, 0, sizeof(*s));
    if (!blob || len < HEAD_SIZE) return false;
    const uint8_t *p = (const uint8_t *)blob;
    if (memcmp(p, "LSPR", 4) != 0) return false;
    if (rd16(p + 4) != 1) return false;

    s->blob     = p;
    s->len      = len;
    s->angles   = rd16(p + 6);
    s->poses    = rd16(p + 8);
    s->anchor_x = rds16(p + 12);
    s->anchor_y = rds16(p + 14);
    memcpy(&s->ref_height, p + 16, sizeof(float));
    s->table    = p + 20;

    if (s->angles <= 0 || s->poses <= 0) return false;
    size_t need = 20 + (size_t)s->angles * s->poses * ENTRY_SIZE;
    if (len < need) return false;
    return true;
}

static const uint8_t *entry_of(const g3d_sprite_set *s, int angle, int pose)
{
    if (angle < 0) angle = 0;
    if (pose < 0) pose = 0;
    if (angle >= s->angles) angle %= s->angles;
    if (pose >= s->poses) pose = s->poses - 1;
    return s->table + ((size_t)pose * s->angles + angle) * ENTRY_SIZE;
}

bool g3d_sprite_size(const g3d_sprite_set *s, int angle, int pose,
                     int *w, int *h, int *ox, int *oy)
{
    if (!s->blob) return false;
    const uint8_t *e = entry_of(s, angle, pose);
    if (ox) *ox = rds16(e + 0);
    if (oy) *oy = rds16(e + 2);
    if (w)  *w  = rd16(e + 4);
    if (h)  *h  = rd16(e + 6);
    return true;
}

int g3d_sprite_angle_index(const g3d_sprite_set *s, float radians)
{
    const float TAU = 6.2831853f;
    float step = TAU / (float)s->angles;
    /* Redondeo al angulo mas cercano, normalizando primero a [0, 2pi). */
    float a = radians;
    while (a < 0.f) a += TAU;
    while (a >= TAU) a -= TAU;
    int idx = (int)(a / step + 0.5f);
    if (idx >= s->angles) idx -= s->angles;
    return idx;
}

void g3d_sprite_draw(g3d_target *t, const g3d_sprite_set *s, int angle, int pose,
                     float ax, float ay, float scale, float scale_y, float shear,
                     float invw, float tint, g3d_color tint_b, uint8_t tint_a)
{
    if (!s->blob || scale <= 0.f || scale_y <= 0.f) return;

    const uint8_t *e = entry_of(s, angle, pose);
    const int   ox = rds16(e + 0), oy = rds16(e + 2);
    const int   sw = rd16(e + 4),  sh = rd16(e + 6);
    const uint32_t off = rd32(e + 8);
    if (sw <= 0 || sh <= 0) return;

    const size_t color_bytes = (size_t)sw * sh * 2;
    const size_t alpha_bytes = ((size_t)sw * sh + 1) / 2;
    if (off + color_bytes + alpha_bytes > s->len) return;

    const uint8_t *cdata = s->blob + off;
    const uint8_t *adata = cdata + color_bytes;

    /* Rectangulo de destino. El anclaje esta entre las patas, asi que escalar
     * en Y alrededor de el deja los pies plantados en el piso. */
    const int dx0 = (int)(ax + (float)ox * scale + 0.5f);
    const int dy0 = (int)(ay + (float)oy * scale * scale_y + 0.5f);
    const int dw = (int)((float)sw * scale + 0.5f);
    const int dh = (int)((float)sh * scale * scale_y + 0.5f);
    if (dw <= 0 || dh <= 0) return;

    /* Paso en el origen, en 16.16. */
    const uint32_t step_x = (uint32_t)(((int64_t)sw << 16) / dw);
    const uint32_t step_y = (uint32_t)(((int64_t)sh << 16) / dh);

    int cy0 = dy0, ch = dh;
    uint32_t src_y0 = 0;
    if (cy0 < 0) { src_y0 = (uint32_t)(-cy0) * step_y; ch += cy0; cy0 = 0; }
    if (cy0 + ch > t->h) ch = t->h - cy0;
    if (ch <= 0) return;

    const bool shaded = (tint < 0.995f || tint > 1.005f);
    const bool tinted = (tint_a > 0);
    const float ta = tint_a / 255.f;
    const float shear_px = shear * (float)dh;
    uint32_t sy = src_y0;

    for (int y = 0; y < ch; y++) {
        const uint32_t row = (sy >> 16) < (uint32_t)sh ? (sy >> 16) : (uint32_t)(sh - 1);
        const uint8_t *crow = cdata + (size_t)row * sw * 2;
        const size_t   abase = (size_t)row * sw;
        sy += step_y;

        /* La cizalla corre la fila entera: maxima arriba, nula en los pies. */
        const int full_y = cy0 + y - dy0;
        int rx0 = dx0 + (int)(shear_px * (float)(dh - 1 - full_y) / (float)dh);
        int rw = dw;
        uint32_t sx = 0;
        if (rx0 < 0) { sx = (uint32_t)(-rx0) * step_x; rw += rx0; rx0 = 0; }
        if (rx0 + rw > t->w) rw = t->w - rx0;
        if (rw <= 0) continue;

        g3d_color *dst = t->color + (size_t)(cy0 + y) * t->w + rx0;
        float     *dep = t->depth + (size_t)(cy0 + y) * t->w + rx0;

        for (int x = 0; x < rw; x++) {
            const uint32_t col = (sx >> 16) < (uint32_t)sw ? (sx >> 16) : (uint32_t)(sw - 1);
            const size_t   ai = abase + col;
            const uint8_t  nib = adata[ai >> 1];
            const uint8_t  a = (ai & 1) ? (uint8_t)(nib >> 4) : (uint8_t)(nib & 0x0F);
            sx += step_x;
            if (a == 0) continue;
            if (invw <= dep[x]) continue;          /* algo mas cercano ya lo tapa */

            g3d_color c = (g3d_color)(crow[col * 2] | (crow[col * 2 + 1] << 8));
            if (shaded) c = g3d_color_shade(c, tint);
            if (tinted) c = g3d_color_lerp(c, tint_b, ta);

            if (a >= 15) {
                dst[x] = c;
                dep[x] = invw;
            } else {
                dst[x] = g3d_color_lerp(dst[x], c, (float)a * (1.f / 15.f));
                if (a >= 8) dep[x] = invw;
            }
        }
    }
}
