/*
 * g2d - dibujo 2D sobre un g3d_target (HUD, botones, texto).
 * Ignora el z-buffer: se usa siempre despues del render 3D.
 */
#ifndef G2D_H
#define G2D_H

#include "g3d.h"

#ifdef __cplusplus
extern "C" {
#endif

void g2d_pixel(g3d_target *t, int x, int y, g3d_color c);
void g2d_blend_pixel(g3d_target *t, int x, int y, g3d_color c, uint8_t alpha);
void g2d_hline(g3d_target *t, int x, int y, int w, g3d_color c);
void g2d_vline(g3d_target *t, int x, int y, int h, g3d_color c);
void g2d_line(g3d_target *t, int x0, int y0, int x1, int y1, g3d_color c);
void g2d_fill_rect(g3d_target *t, int x, int y, int w, int h, g3d_color c);
void g2d_blend_rect(g3d_target *t, int x, int y, int w, int h, g3d_color c, uint8_t alpha);
void g2d_rect(g3d_target *t, int x, int y, int w, int h, g3d_color c);
void g2d_fill_round_rect(g3d_target *t, int x, int y, int w, int h, int r, g3d_color c);
void g2d_round_rect(g3d_target *t, int x, int y, int w, int h, int r, g3d_color c);
void g2d_fill_circle(g3d_target *t, int cx, int cy, int r, g3d_color c);
void g2d_circle(g3d_target *t, int cx, int cy, int r, g3d_color c);
void g2d_fill_ellipse(g3d_target *t, int cx, int cy, int rx, int ry, g3d_color c);
void g2d_blend_ellipse(g3d_target *t, int cx, int cy, int rx, int ry, g3d_color c, uint8_t alpha);
void g2d_fill_tri(g3d_target *t, int x0, int y0, int x1, int y1, int x2, int y2, g3d_color c);

/* Texto: fuente 5x7, soporta UTF-8 para a e i o u con tilde, n con virgulilla y ¡ ¿ */
void g2d_text(g3d_target *t, int x, int y, int scale, g3d_color c, const char *s);
void g2d_text_shadow(g3d_target *t, int x, int y, int scale, g3d_color c, g3d_color sh, const char *s);
int  g2d_text_width(int scale, const char *s);
void g2d_text_center(g3d_target *t, int cx, int y, int scale, g3d_color c, const char *s);

#ifdef __cplusplus
}
#endif
#endif /* G2D_H */
