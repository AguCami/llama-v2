/*
 * Primitivas 2D + fuente 5x7 con soporte UTF-8 para castellano.
 */
#include "g2d.h"
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ fuente */
/* 5 columnas por glifo, bit 0 = fila superior. ASCII 0x20..0x7E. */
static const uint8_t FONT5X7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /*   */ {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */ {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */ {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */ {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */ {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* * */ {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */ {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */ {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */ {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */ {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */ {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */ {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */ {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */ {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x08,0x14,0x22,0x41,0x00}, /* < */ {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x00,0x41,0x22,0x14,0x08}, /* > */ {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */ {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */ {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */ {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */ {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */ {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */ {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */ {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */ {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */ {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */ {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */ {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */ {0x3F,0x40,0x38,0x40,0x3F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */ {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */ {0x00,0x7F,0x41,0x41,0x00}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* \ */ {0x00,0x41,0x41,0x7F,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */ {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */ {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */ {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */ {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */ {0x58,0x64,0x64,0x64,0x3C}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */ {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */ {0x7F,0x10,0x28,0x44,0x00}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */ {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */ {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */ {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */ {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */ {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */ {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */ {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}, /* z */ {0x00,0x08,0x36,0x41,0x00}, /* { */
    {0x00,0x00,0x7F,0x00,0x00}, /* | */ {0x00,0x41,0x36,0x08,0x00}, /* } */
    {0x08,0x08,0x2A,0x1C,0x08}, /* ~ */
};

/* Glifos extendidos: a e i o u con tilde, enie, ¡, ¿, grado. */
enum { GX_AA = 0, GX_EA, GX_IA, GX_OA, GX_UA, GX_NT, GX_EXI, GX_QUI, GX_DEG, GX_COUNT };

static const uint8_t FONT_EXT[GX_COUNT][5] = {
    {0x20,0x56,0x55,0x54,0x78}, /* a con tilde */
    {0x38,0x56,0x55,0x54,0x18}, /* e con tilde */
    {0x00,0x46,0x7D,0x40,0x00}, /* i con tilde */
    {0x38,0x46,0x45,0x44,0x38}, /* o con tilde */
    {0x3C,0x42,0x41,0x20,0x7C}, /* u con tilde */
    {0x7E,0x09,0x05,0x06,0x7A}, /* enie */
    {0x00,0x00,0x7D,0x00,0x00}, /* ¡ */
    {0x30,0x48,0x45,0x40,0x20}, /* ¿ */
    {0x00,0x07,0x05,0x07,0x00}, /* grado */
};

/* Devuelve el glifo del siguiente caracter y avanza el puntero. */
static const uint8_t *next_glyph(const char **ps)
{
    const unsigned char *s = (const unsigned char *)*ps;
    unsigned char c = *s++;
    if (c < 0x80) {
        *ps = (const char *)s;
        if (c < 0x20 || c > 0x7E) return FONT5X7[0];
        return FONT5X7[c - 0x20];
    }
    unsigned char c2 = *s ? *s++ : 0;
    *ps = (const char *)s;
    if (c == 0xC3) {
        switch (c2) {
        case 0xA1: return FONT_EXT[GX_AA];
        case 0xA9: return FONT_EXT[GX_EA];
        case 0xAD: return FONT_EXT[GX_IA];
        case 0xB3: return FONT_EXT[GX_OA];
        case 0xBA: return FONT_EXT[GX_UA];
        case 0xB1: return FONT_EXT[GX_NT];
        case 0x81: return FONT5X7['A' - 0x20];
        case 0x89: return FONT5X7['E' - 0x20];
        case 0x8D: return FONT5X7['I' - 0x20];
        case 0x93: return FONT5X7['O' - 0x20];
        case 0x9A: return FONT5X7['U' - 0x20];
        case 0x91: return FONT5X7['N' - 0x20];
        default: break;
        }
    } else if (c == 0xC2) {
        if (c2 == 0xA1) return FONT_EXT[GX_EXI];
        if (c2 == 0xBF) return FONT_EXT[GX_QUI];
        if (c2 == 0xB0) return FONT_EXT[GX_DEG];
    }
    return FONT5X7['?' - 0x20];
}

/* ---------------------------------------------------------------- pixeles */

void g2d_pixel(g3d_target *t, int x, int y, g3d_color c)
{
    if (x < 0 || y < 0 || x >= t->w || y >= t->h) return;
    t->color[(size_t)y * t->w + x] = c;
}

void g2d_blend_pixel(g3d_target *t, int x, int y, g3d_color c, uint8_t alpha)
{
    if (x < 0 || y < 0 || x >= t->w || y >= t->h) return;
    size_t i = (size_t)y * t->w + x;
    t->color[i] = g3d_color_lerp(t->color[i], c, alpha / 255.f);
}

void g2d_hline(g3d_target *t, int x, int y, int w, g3d_color c)
{
    if (y < 0 || y >= t->h) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > t->w) w = t->w - x;
    if (w <= 0) return;
    g3d_color *p = t->color + (size_t)y * t->w + x;
    for (int i = 0; i < w; i++) p[i] = c;
}

void g2d_vline(g3d_target *t, int x, int y, int h, g3d_color c)
{
    if (x < 0 || x >= t->w) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > t->h) h = t->h - y;
    for (int i = 0; i < h; i++) t->color[(size_t)(y + i) * t->w + x] = c;
}

void g2d_line(g3d_target *t, int x0, int y0, int x1, int y1, g3d_color c)
{
    int dx = x1 - x0, dy = y1 - y0;
    int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    int steps = adx > ady ? adx : ady;
    if (steps == 0) { g2d_pixel(t, x0, y0, c); return; }
    for (int i = 0; i <= steps; i++) {
        g2d_pixel(t, x0 + dx * i / steps, y0 + dy * i / steps, c);
    }
}

void g2d_fill_rect(g3d_target *t, int x, int y, int w, int h, g3d_color c)
{
    for (int i = 0; i < h; i++) g2d_hline(t, x, y + i, w, c);
}

void g2d_blend_rect(g3d_target *t, int x, int y, int w, int h, g3d_color c, uint8_t alpha)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > t->w) w = t->w - x;
    if (y + h > t->h) h = t->h - y;
    float a = alpha / 255.f;
    for (int j = 0; j < h; j++) {
        g3d_color *p = t->color + (size_t)(y + j) * t->w + x;
        for (int i = 0; i < w; i++) p[i] = g3d_color_lerp(p[i], c, a);
    }
}

void g2d_rect(g3d_target *t, int x, int y, int w, int h, g3d_color c)
{
    g2d_hline(t, x, y, w, c);
    g2d_hline(t, x, y + h - 1, w, c);
    g2d_vline(t, x, y, h, c);
    g2d_vline(t, x + w - 1, y, h, c);
}

void g2d_fill_round_rect(g3d_target *t, int x, int y, int w, int h, int r, g3d_color c)
{
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    g2d_fill_rect(t, x + r, y, w - 2 * r, h, c);
    for (int i = 0; i < r; i++) {
        int dy = r - 1 - i;
        int dx = 0;
        while ((dx + 1) * (dx + 1) + dy * dy <= r * r) dx++;
        int inset = r - dx;
        g2d_hline(t, x + inset, y + i, w - 2 * inset, c);
        g2d_hline(t, x + inset, y + h - 1 - i, w - 2 * inset, c);
    }
}

void g2d_round_rect(g3d_target *t, int x, int y, int w, int h, int r, g3d_color c)
{
    g2d_hline(t, x + r, y, w - 2 * r, c);
    g2d_hline(t, x + r, y + h - 1, w - 2 * r, c);
    g2d_vline(t, x, y + r, h - 2 * r, c);
    g2d_vline(t, x + w - 1, y + r, h - 2 * r, c);
    for (int i = 0; i < r; i++) {
        int dy = r - 1 - i;
        int dx = 0;
        while ((dx + 1) * (dx + 1) + dy * dy <= r * r) dx++;
        int inset = r - dx;
        g2d_pixel(t, x + inset, y + i, c);
        g2d_pixel(t, x + w - 1 - inset, y + i, c);
        g2d_pixel(t, x + inset, y + h - 1 - i, c);
        g2d_pixel(t, x + w - 1 - inset, y + h - 1 - i, c);
    }
}

void g2d_fill_circle(g3d_target *t, int cx, int cy, int r, g3d_color c)
{
    g2d_fill_ellipse(t, cx, cy, r, r, c);
}

void g2d_circle(g3d_target *t, int cx, int cy, int r, g3d_color c)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        g2d_pixel(t, cx + x, cy + y, c); g2d_pixel(t, cx + y, cy + x, c);
        g2d_pixel(t, cx - y, cy + x, c); g2d_pixel(t, cx - x, cy + y, c);
        g2d_pixel(t, cx - x, cy - y, c); g2d_pixel(t, cx - y, cy - x, c);
        g2d_pixel(t, cx + y, cy - x, c); g2d_pixel(t, cx + x, cy - y, c);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

void g2d_fill_ellipse(g3d_target *t, int cx, int cy, int rx, int ry, g3d_color c)
{
    if (rx <= 0 || ry <= 0) return;
    for (int y = -ry; y <= ry; y++) {
        int span = (int)(rx * sqrtf(1.f - (float)(y * y) / (float)(ry * ry)));
        g2d_hline(t, cx - span, cy + y, span * 2 + 1, c);
    }
}

void g2d_blend_ellipse(g3d_target *t, int cx, int cy, int rx, int ry, g3d_color c, uint8_t alpha)
{
    if (rx <= 0 || ry <= 0) return;
    float a = alpha / 255.f;
    for (int y = -ry; y <= ry; y++) {
        int py = cy + y;
        if (py < 0 || py >= t->h) continue;
        int span = (int)(rx * sqrtf(1.f - (float)(y * y) / (float)(ry * ry)));
        int x0 = cx - span, x1 = cx + span;
        if (x0 < 0) x0 = 0;
        if (x1 > t->w - 1) x1 = t->w - 1;
        g3d_color *row = t->color + (size_t)py * t->w;
        for (int x = x0; x <= x1; x++) row[x] = g3d_color_lerp(row[x], c, a);
    }
}

void g2d_fill_tri(g3d_target *t, int x0, int y0, int x1, int y1, int x2, int y2, g3d_color c)
{
    int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    if (miny < 0) miny = 0;
    if (maxy > t->h - 1) maxy = t->h - 1;
    for (int y = miny; y <= maxy; y++) {
        int xs[3], n = 0;
        int px[3] = { x0, x1, x2 }, py[3] = { y0, y1, y2 };
        for (int e = 0; e < 3; e++) {
            int a = e, b = (e + 1) % 3;
            if ((py[a] <= y && py[b] > y) || (py[b] <= y && py[a] > y)) {
                xs[n++] = px[a] + (y - py[a]) * (px[b] - px[a]) / (py[b] - py[a]);
            }
        }
        if (n >= 2) {
            int xa = xs[0] < xs[1] ? xs[0] : xs[1];
            int xb = xs[0] < xs[1] ? xs[1] : xs[0];
            g2d_hline(t, xa, y, xb - xa + 1, c);
        }
    }
}

/* ------------------------------------------------------------------ texto */

void g2d_text(g3d_target *t, int x, int y, int scale, g3d_color c, const char *s)
{
    if (scale < 1) scale = 1;
    int cx = x;
    while (*s) {
        if (*s == '\n') { s++; cx = x; y += 8 * scale; continue; }
        const uint8_t *g = next_glyph(&s);
        for (int col = 0; col < 5; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1u << row)) {
                    if (scale == 1) g2d_pixel(t, cx + col, y + row, c);
                    else g2d_fill_rect(t, cx + col * scale, y + row * scale, scale, scale, c);
                }
            }
        }
        cx += 6 * scale;
    }
}

void g2d_text_shadow(g3d_target *t, int x, int y, int scale, g3d_color c, g3d_color sh, const char *s)
{
    g2d_text(t, x + scale, y + scale, scale, sh, s);
    g2d_text(t, x, y, scale, c, s);
}

int g2d_text_width(int scale, const char *s)
{
    if (scale < 1) scale = 1;
    int n = 0;
    while (*s) {
        if ((*(const unsigned char *)s & 0xC0) != 0x80) n++;
        s++;
    }
    return n * 6 * scale - scale;
}

void g2d_text_center(g3d_target *t, int cx, int y, int scale, g3d_color c, const char *s)
{
    g2d_text(t, cx - g2d_text_width(scale, s) / 2, y, scale, c, s);
}
