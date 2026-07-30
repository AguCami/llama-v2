/*
 * g3d_pano - fondo panoramico de 360 grados.
 *
 * El paisaje lejano no necesita geometria: al orbitar la camara solo rota, no
 * cambia de paralaje, asi que una tira de pixeles que se desplaza segun el
 * azimut da el resultado exacto y sale mucho mas barato que la cordillera 3D.
 *
 * La tira se genera con tools/mkpano.py a partir de las imagenes de Higgsfield,
 * una por estacion, y se mapea desde la flash.
 */
#ifndef G3D_PANO_H
#define G3D_PANO_H

#include "g3d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t *blob;
    size_t         len;
    int            count;      /* cantidad de estaciones */
    int            width;      /* ancho de la tira (360 grados) */
    int            height;     /* alto de la tira */
    int            horizon;    /* fila de la tira donde cae el horizonte */
    const uint8_t *table;      /* offsets, 4 bytes por estacion */
} g3d_pano_set;

bool g3d_pano_open(g3d_pano_set *s, const void *blob, size_t len);

/*
 * Dibuja la tira `index` de forma que su horizonte quede en la fila
 * `screen_horizon`, desplazada segun `yaw` (radianes, el azimut de la camara).
 * `tint_a`/`tint_b` mezclan hacia un color de ambiente: `tint_b` es el color y
 * `tint_a` cuanto pesa (0 = imagen tal cual, 255 = solo el color). Con eso se
 * hacen el amanecer, el atardecer y la noche desde una sola imagen.
 * `shade` multiplica el brillo antes de la mezcla.
 */
void g3d_pano_draw(g3d_target *t, const g3d_pano_set *s, int index,
                   float yaw, int screen_horizon,
                   float shade, g3d_color tint_b, uint8_t tint_a);

#ifdef __cplusplus
}
#endif
#endif /* G3D_PANO_H */
