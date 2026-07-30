/*
 * Copiado del fondo panoramico: desplazamiento circular segun el azimut y
 * mezcla de ambiente para la hora del dia.
 */
#include "g3d_pano.h"
#include <string.h>

#define PANO_HEAD 20

static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

bool g3d_pano_open(g3d_pano_set *s, const void *blob, size_t len)
{
    memset(s, 0, sizeof(*s));
    if (!blob || len < PANO_HEAD) return false;
    const uint8_t *p = (const uint8_t *)blob;
    if (memcmp(p, "LPAN", 4) != 0) return false;
    if (rd16(p + 4) != 1) return false;

    s->blob    = p;
    s->len     = len;
    s->count   = rd16(p + 6);
    s->width   = rd16(p + 8);
    s->height  = rd16(p + 10);
    s->horizon = rd16(p + 12);
    s->table   = p + PANO_HEAD;

    if (s->count <= 0 || s->width <= 0 || s->height <= 0) return false;
    if (s->horizon < 0 || s->horizon >= s->height) return false;
    if (len < PANO_HEAD + (size_t)s->count * 4) return false;
    return true;
}

void g3d_pano_draw(g3d_target *t, const g3d_pano_set *s, int index,
                   float yaw, int screen_horizon,
                   float shade, g3d_color tint_b, uint8_t tint_a)
{
    if (!s->blob) return;
    if (index < 0 || index >= s->count) index = 0;

    const uint32_t off = rd32(s->table + index * 4);
    const size_t need = (size_t)s->width * s->height * 2;
    if (off + need > s->len) return;
    const uint8_t *pix = s->blob + off;

    /* Desplazamiento circular: una vuelta completa recorre la tira entera. */
    const float TAU = 6.2831853f;
    float turn = yaw / TAU;
    turn -= (float)(int)turn;
    if (turn < 0.f) turn += 1.f;
    int shift = (int)(turn * (float)s->width);

    /* Fila de la tira que cae en la fila 0 de la pantalla. */
    const int top_row = s->horizon - screen_horizon;

    const bool do_shade = (shade < 0.995f || shade > 1.005f);
    const bool do_tint  = (tint_a > 0);
    const float ta = tint_a / 255.f;

    for (int y = 0; y < t->h; y++) {
        int row = top_row + y;
        if (row < 0) row = 0;
        if (row >= s->height) break;          /* debajo del horizonte manda el 3D */

        const uint8_t *src = pix + (size_t)row * s->width * 2;
        g3d_color *dst = t->color + (size_t)y * t->w;

        int sx = shift;
        for (int x = 0; x < t->w; x++) {
            g3d_color c = (g3d_color)(src[sx * 2] | (src[sx * 2 + 1] << 8));
            if (do_shade) c = g3d_color_shade(c, shade);
            if (do_tint)  c = g3d_color_lerp(c, tint_b, ta);
            dst[x] = c;
            if (++sx >= s->width) sx = 0;
        }
    }
}
