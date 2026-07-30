/*
 * g3d_ground - piso texturizado al estilo "Mode 7".
 *
 * Para un plano horizontal visto por una camara sin balanceo, cada fila de la
 * pantalla cae sobre una recta del plano y el paso de textura es constante a lo
 * largo de la fila: el bucle interno son dos sumas y una lectura. Ademas la
 * profundidad es constante por fila, asi que el z-buffer se llena igual que lo
 * haria la malla del piso y todo lo demas se apoya encima sin cambios.
 *
 * La textura la genera tools/mkground.py a partir de las imagenes de
 * Higgsfield (una por estacion) y se mapea desde la flash.
 */
#ifndef G3D_GROUND_H
#define G3D_GROUND_H

#include "g3d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t *blob;
    size_t         len;
    int            count;   /* cantidad de estaciones */
    int            size;    /* lado de la baldosa, potencia de dos */
    int            shift;   /* log2(size) */
    float          span;    /* cuantas unidades de mundo cubre una baldosa */
    const uint8_t *table;   /* offsets, 4 bytes por estacion */
} g3d_ground_set;

bool g3d_ground_open(g3d_ground_set *s, const void *blob, size_t len);

/*
 * Dibuja la baldosa `index` como piso infinito (plano y = 0) desde la camara
 * de `ctx`, escribiendo color y profundidad desde `y_top` hasta el fondo del
 * blanco. Aplica la niebla de `ctx->light` por fila y el ambiente de la hora
 * (`shade`, `tint_b`, `tint_a`) igual que el panorama.
 */
void g3d_ground_draw(g3d_target *t, const g3d_ctx *ctx, const g3d_ground_set *s,
                     int index, int y_top,
                     float shade, g3d_color tint_b, uint8_t tint_a);

#ifdef __cplusplus
}
#endif
#endif /* G3D_GROUND_H */
