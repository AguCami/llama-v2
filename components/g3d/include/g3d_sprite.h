/*
 * g3d_sprite - hojas de sprites pre-renderizadas.
 *
 * El modelo de la llama tiene 29.314 triangulos: imposible dibujarlo en tiempo
 * real en el ESP32-S3. En cambio se renderiza en la computadora desde N angulos
 * (tools/mksprites.py) y la placa solo copia pixeles. Lo que se ve en pantalla
 * es el modelo original, no una aproximacion.
 *
 * El blob se mapea directo desde la flash: no ocupa RAM.
 */
#ifndef G3D_SPRITE_H
#define G3D_SPRITE_H

#include "g3d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t *blob;
    size_t         len;
    int            angles;      /* cantidad de angulos */
    int            poses;       /* cantidad de poses por angulo */
    int            anchor_x;    /* anclaje con el que se renderizo */
    int            anchor_y;
    float          ref_height;  /* alto del modelo en unidades de mundo */
    const uint8_t *table;       /* tabla de entradas de 12 bytes */
} g3d_sprite_set;

/* Valida la cabecera. Devuelve false si el blob no es una hoja valida. */
bool g3d_sprite_open(g3d_sprite_set *s, const void *blob, size_t len);

/* Tamano de un sprite (en pixeles del render original). */
bool g3d_sprite_size(const g3d_sprite_set *s, int angle, int pose,
                     int *w, int *h, int *ox, int *oy);

/*
 * Dibuja el sprite anclado en (ax, ay) de la pantalla, escalado por `scale`.
 * Respeta el z-buffer: compara y escribe `invw` (1/w del centro del objeto),
 * asi los objetos mas cercanos del corral siguen tapandolo.
 * `tint` multiplica el brillo (para la noche o la llama enferma) y
 * `tint_b`/`tint_a` mezclan el color de ambiente de la hora, igual que en el
 * panorama y en g3d_light.
 *
 * `scale_y` y `shear` son los canales de titere, para el rebote del trote, la
 * respiracion y los saltos: scale_y aplasta o estira alrededor del anclaje
 * (los pies quedan plantados) y shear corre la parte de arriba en X como
 * fraccion del alto (0.1 = el 10 % del alto hacia la derecha).
 *
 * Agachar el cuello NO se hace con estos canales: deformar una foto hunde la
 * cabeza dentro del cuerpo en vez de bajarla. Para eso estan las poses, que
 * salen de deformar la geometria antes de renderizar (ver tools/mksprites.py).
 */
void g3d_sprite_draw(g3d_target *t, const g3d_sprite_set *s, int angle, int pose,
                     float ax, float ay, float scale, float scale_y,
                     float shear, float invw, float tint,
                     g3d_color tint_b, uint8_t tint_a);

/* Indice de angulo mas cercano a `radians` (0 = mirando a +Z). */
int g3d_sprite_angle_index(const g3d_sprite_set *s, float radians);

#ifdef __cplusplus
}
#endif
#endif /* G3D_SPRITE_H */
