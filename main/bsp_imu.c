/*
 * IMU QMI8658 de 6 ejes (I2C 0x6B o 0x6A).
 *
 * Configuracion: acelerometro +-4 g y giroscopo +-512 dps, ambos a 117 Hz,
 * suficiente para detectar sacudidas e inclinacion.
 */
#include "bsp.h"
#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_imu";

#define QMI_WHO_AM_I 0x00
#define QMI_CTRL1    0x02
#define QMI_CTRL2    0x03
#define QMI_CTRL3    0x04
#define QMI_CTRL7    0x08
#define QMI_AX_L     0x35

#define ACC_LSB_PER_G   8192.f    /* +-4 g  */
#define GYR_LSB_PER_DPS 64.f      /* +-512 dps */

static uint8_t s_addr;

esp_err_t bsp_imu_init(void)
{
    const uint8_t candidates[2] = { BOARD_IMU_ADDR_L, BOARD_IMU_ADDR_H };
    s_addr = 0;
    for (int i = 0; i < 2; i++) {
        uint8_t who = 0;
        if (bsp_i2c_read_reg(candidates[i], QMI_WHO_AM_I, &who, 1) == ESP_OK && who == 0x05) {
            s_addr = candidates[i];
            break;
        }
    }
    if (!s_addr) {
        ESP_LOGW(TAG, "QMI8658 no encontrado");
        return ESP_ERR_NOT_FOUND;
    }

    bsp_i2c_write_reg(s_addr, QMI_CTRL1, 0x40);   /* auto incremento de registro */
    bsp_i2c_write_reg(s_addr, QMI_CTRL2, 0x15);   /* acc +-4 g, 117 Hz */
    bsp_i2c_write_reg(s_addr, QMI_CTRL3, 0x55);   /* gyr +-512 dps, 117 Hz */
    bsp_i2c_write_reg(s_addr, QMI_CTRL7, 0x03);   /* habilita acc + gyr */
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_LOGI(TAG, "IMU lista en 0x%02X", s_addr);
    return ESP_OK;
}

bool bsp_imu_read(float *ax, float *ay, float *az, float *gx, float *gy, float *gz)
{
    if (!s_addr) return false;
    uint8_t d[12];
    if (bsp_i2c_read_reg(s_addr, QMI_AX_L, d, sizeof(d)) != ESP_OK) return false;

    int16_t raw[6];
    for (int i = 0; i < 6; i++) {
        raw[i] = (int16_t)((uint16_t)d[i * 2] | ((uint16_t)d[i * 2 + 1] << 8));
    }
    if (ax) *ax = raw[0] / ACC_LSB_PER_G;
    if (ay) *ay = raw[1] / ACC_LSB_PER_G;
    if (az) *az = raw[2] / ACC_LSB_PER_G;
    if (gx) *gx = raw[3] / GYR_LSB_PER_DPS;
    if (gy) *gy = raw[4] / GYR_LSB_PER_DPS;
    if (gz) *gz = raw[5] / GYR_LSB_PER_DPS;
    return true;
}
