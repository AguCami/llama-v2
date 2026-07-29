/*
 * Capa de plataforma: lo unico que el juego necesita del sistema operativo.
 * Cada destino (ESP32-S3 o simulador de escritorio) implementa estas funciones.
 */
#ifndef LLAMA_PLATFORM_H
#define LLAMA_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hora actual en segundos unix (RTC del sistema). */
double llama_plat_time(void);

/* Milisegundos monotonicos desde el arranque. */
uint32_t llama_plat_millis(void);

/* Persistencia de la partida (NVS en el ESP32, archivo en el simulador). */
bool llama_plat_save(const void *blob, size_t len);
bool llama_plat_load(void *blob, size_t len);

/* Pitido corto del buzzer/codec. freq_hz = 0 -> silencio. */
void llama_plat_tone(int freq_hz, int ms);

/* Vibracion/haptico si el destino lo soporta (opcional). */
void llama_plat_haptic(int ms);

/* Semilla de entropia para el generador pseudoaleatorio. */
uint32_t llama_plat_seed(void);

#ifdef __cplusplus
}
#endif
#endif /* LLAMA_PLATFORM_H */
