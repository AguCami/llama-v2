/*
 * Estacion y hora del dia a partir del reloj.
 */
#include "llama_ambient.h"
#include <math.h>

/* Fecha civil desde dias unix (algoritmo de Howard Hinnant). */
void llama_civil_from_unix(double unix_time, int *year, int *month, int *day,
                           float *hour)
{
    double days_f = floor(unix_time / 86400.0);
    double rem = unix_time - days_f * 86400.0;
    long z = (long)days_f + 719468;

    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned long doe = (unsigned long)(z - era * 146097);
    unsigned long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y = (long)yoe + era * 400;
    unsigned long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned long mp = (5 * doy + 2) / 153;
    unsigned long d = doy - (153 * mp + 2) / 5 + 1;
    unsigned long m = mp + (mp < 10 ? 3 : -9);

    if (year)  *year = (int)(y + (m <= 2 ? 1 : 0));
    if (month) *month = (int)m;
    if (day)   *day = (int)d;
    if (hour)  *hour = (float)(rem / 3600.0);
}

static llama_season season_of_month(int month)
{
    /* Trimestres astronomicos aproximados. */
    static const llama_season NORTH[12] = {
        SEASON_INVIERNO, SEASON_INVIERNO,                   /* ene, feb */
        SEASON_PRIMAVERA, SEASON_PRIMAVERA, SEASON_PRIMAVERA,
        SEASON_VERANO, SEASON_VERANO, SEASON_VERANO,
        SEASON_OTONIO, SEASON_OTONIO, SEASON_OTONIO,
        SEASON_INVIERNO,                                    /* dic */
    };
    if (month < 1) month = 1;
    if (month > 12) month = 12;
    llama_season s = NORTH[month - 1];
#if LLAMA_SOUTHERN_HEMISPHERE
    /* En el sur las estaciones van corridas medio ano. */
    switch (s) {
    case SEASON_INVIERNO:  s = SEASON_VERANO;   break;
    case SEASON_VERANO:    s = SEASON_INVIERNO; break;
    case SEASON_PRIMAVERA: s = SEASON_OTONIO;   break;
    default:               s = SEASON_PRIMAVERA; break;
    }
#endif
    return s;
}

const char *llama_season_name(llama_season s)
{
    switch (s) {
    case SEASON_VERANO:    return "Verano";
    case SEASON_OTONIO:    return "Otoño";
    case SEASON_INVIERNO:  return "Invierno";
    case SEASON_PRIMAVERA: return "Primavera";
    default:               return "?";
    }
}

/* --------------------------------------------------------- hora del dia */

typedef struct {
    float hour;
    float shade;
    uint8_t r, g, b, a;         /* color de ambiente y su peso */
    uint8_t fr, fg, fb;         /* color del horizonte */
    uint8_t tr, tg, tb;         /* cielo arriba */
    uint8_t br, bg, bb;         /* cielo abajo */
    float night;
} phase_key;

/* Amanecer, dia, atardecer y noche. La lista tiene que estar ordenada por hora
 * y cerrar el ciclo (la ultima entrada repite la primera a las 24). */
static const phase_key KEYS[] = {
    {  0.0f, 0.42f,  16,  20,  54, 170,  40,  44,  86,  10,  14,  42,  48,  46,  88, 1.00f },
    {  4.8f, 0.44f,  22,  26,  62, 165,  58,  54,  98,  16,  20,  52,  70,  60, 102, 0.95f },
    {  6.3f, 0.64f, 255, 140,  90, 105, 240, 170, 130,  60,  70, 130, 250, 180, 140, 0.50f },
    {  8.0f, 1.00f, 255, 255, 255,   0, 206, 232, 246, 104, 176, 236, 206, 232, 246, 0.00f },
    { 17.5f, 1.00f, 255, 255, 255,   0, 206, 232, 246, 104, 176, 236, 206, 232, 246, 0.00f },
    { 19.6f, 0.80f, 255, 120,  60, 115, 250, 160, 110,  70,  80, 150, 250, 170, 120, 0.32f },
    { 21.3f, 0.48f,  30,  28,  70, 158,  60,  52,  96,  20,  24,  58,  64,  58, 100, 0.88f },
    { 24.0f, 0.42f,  16,  20,  54, 170,  40,  44,  86,  10,  14,  42,  48,  46,  88, 1.00f },
};
#define NKEYS ((int)(sizeof(KEYS) / sizeof(KEYS[0])))

static uint8_t lerp8(uint8_t a, uint8_t b, float t)
{
    return (uint8_t)(a + (int)((float)(b - a) * t + (b >= a ? 0.5f : -0.5f)));
}

void llama_ambient_eval(llama_ambient *out, double unix_time)
{
    int month = 1;
    float hour = 12.f;
    llama_civil_from_unix(unix_time, NULL, &month, NULL, &hour);
    if (hour < 0.f) hour = 0.f;
    if (hour > 24.f) hour = 24.f;

    out->month  = month;
    out->season = season_of_month(month);
    out->hour   = hour;

    int i = 0;
    while (i < NKEYS - 2 && hour > KEYS[i + 1].hour) i++;
    const phase_key *a = &KEYS[i], *b = &KEYS[i + 1];
    float span = b->hour - a->hour;
    float t = span > 1e-6f ? (hour - a->hour) / span : 0.f;
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;

    out->shade  = a->shade + (b->shade - a->shade) * t;
    out->night  = a->night + (b->night - a->night) * t;
    out->tint   = g3d_rgb(lerp8(a->r, b->r, t), lerp8(a->g, b->g, t), lerp8(a->b, b->b, t));
    out->tint_a = lerp8(a->a, b->a, t);
    out->fog    = g3d_rgb(lerp8(a->fr, b->fr, t), lerp8(a->fg, b->fg, t), lerp8(a->fb, b->fb, t));
    out->sky_top = g3d_rgb(lerp8(a->tr, b->tr, t), lerp8(a->tg, b->tg, t), lerp8(a->tb, b->tb, t));
    out->sky_bottom = g3d_rgb(lerp8(a->br, b->br, t), lerp8(a->bg, b->bg, t), lerp8(a->bb, b->bb, t));

    /* El invierno enfria y apaga un poco; el verano calienta. */
    if (out->season == SEASON_INVIERNO) {
        out->shade *= 0.95f;
        out->tint_a = (uint8_t)(out->tint_a > 225 ? 255 : out->tint_a + 30);
        out->tint = g3d_color_lerp(out->tint, g3d_rgb(180, 205, 235), 0.25f);
    } else if (out->season == SEASON_VERANO) {
        out->shade *= 1.03f;
        if (out->shade > 1.06f) out->shade = 1.06f;
    }
}
