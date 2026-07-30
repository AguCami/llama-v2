/*
 * Ambiente: estacion del ano y hora del dia, derivadas del reloj de tiempo real.
 *
 * De una sola imagen de paisaje por estacion salen las cuatro horas del dia,
 * porque el amanecer, el atardecer y la noche se consiguen mezclando un color
 * de ambiente sobre el panorama en vez de guardar cuatro imagenes.
 */
#ifndef LLAMA_AMBIENT_H
#define LLAMA_AMBIENT_H

#include "g3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hemisferio sur por defecto: en diciembre es verano. */
#ifndef LLAMA_SOUTHERN_HEMISPHERE
#define LLAMA_SOUTHERN_HEMISPHERE 1
#endif

typedef enum {
    SEASON_VERANO = 0,
    SEASON_OTONIO,
    SEASON_INVIERNO,
    SEASON_PRIMAVERA,
    SEASON_COUNT
} llama_season;

typedef struct {
    llama_season season;
    int          month;       /* 1..12 */
    float        hour;        /* 0..24 continuo */

    float        shade;       /* multiplicador de brillo del ambiente */
    g3d_color    tint;        /* color de ambiente a mezclar */
    uint8_t      tint_a;      /* cuanto pesa la mezcla, 0..255 */
    g3d_color    fog;         /* color del horizonte (niebla de distancia) */
    g3d_color    sky_top;     /* respaldo si no hay panorama */
    g3d_color    sky_bottom;
    float        night;       /* 0 = dia pleno, 1 = noche cerrada */
} llama_ambient;

/* Evalua el ambiente para un instante dado (segundos unix). */
void llama_ambient_eval(llama_ambient *out, double unix_time);

const char *llama_season_name(llama_season s);

/* Descompone segundos unix en fecha civil UTC. */
void llama_civil_from_unix(double unix_time, int *year, int *month, int *day,
                           float *hour);

#ifdef __cplusplus
}
#endif
#endif /* LLAMA_AMBIENT_H */
