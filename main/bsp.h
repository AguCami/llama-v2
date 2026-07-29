/*
 * Capa de soporte de placa (BSP): pantalla, tactil, IMU, RTC y PMU.
 */
#ifndef BSP_H
#define BSP_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------- I2C */
esp_err_t bsp_i2c_init(void);
esp_err_t bsp_i2c_write(uint8_t addr, const uint8_t *data, size_t len);
esp_err_t bsp_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value);
esp_err_t bsp_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
bool      bsp_i2c_probe(uint8_t addr);

/* -------------------------------------------------------------- pantalla */
esp_err_t bsp_display_init(void);
/* Vuelca un framebuffer RGB565 completo (little endian) a la pantalla. */
esp_err_t bsp_display_flush(const uint16_t *fb);
void      bsp_display_backlight(int percent);   /* 0..100 */

/* ---------------------------------------------------------------- tactil */
esp_err_t bsp_touch_init(void);
/* Devuelve true si hay un dedo apoyado; escribe las coordenadas en x/y. */
bool      bsp_touch_read(int *x, int *y);

/* ------------------------------------------------------------------- IMU */
esp_err_t bsp_imu_init(void);
bool      bsp_imu_read(float *ax, float *ay, float *az,
                       float *gx, float *gy, float *gz);

/* ------------------------------------------------------------- PMU y RTC */
esp_err_t bsp_power_init(void);
int       bsp_power_battery_percent(void);      /* -1 si no se pudo leer */
bool      bsp_power_charging(void);

esp_err_t bsp_rtc_init(void);
bool      bsp_rtc_get_time(struct tm *out);
bool      bsp_rtc_set_time(const struct tm *in);
/* Copia la hora del PCF85063 al reloj del sistema. */
void      bsp_rtc_sync_system(void);
/* Guarda la hora del sistema en el PCF85063. */
void      bsp_rtc_store_system(void);

#ifdef __cplusplus
}
#endif
#endif /* BSP_H */
