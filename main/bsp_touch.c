/*
 * Tactil capacitivo CST816T (I2C 0x15).
 *
 * Registros usados:
 *   0x01 GestureID   0x02 FingerNum
 *   0x03 XposH (4 bits bajos)  0x04 XposL
 *   0x05 YposH (4 bits bajos)  0x06 YposL
 *   0xA5 PowerMode   0xFA IrqCtl
 */
#include "bsp.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_touch";

esp_err_t bsp_touch_init(void)
{
    gpio_config_t rst = {
        .pin_bit_mask = 1ULL << BOARD_TP_PIN_RST,
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rst);
    gpio_set_level(BOARD_TP_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(BOARD_TP_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(60));

    gpio_config_t irq = {
        .pin_bit_mask = 1ULL << BOARD_TP_PIN_INT,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&irq);

    if (!bsp_i2c_probe(BOARD_TP_ADDR)) {
        ESP_LOGW(TAG, "CST816T no responde en 0x%02X", BOARD_TP_ADDR);
        return ESP_ERR_NOT_FOUND;
    }
    /* Interrupcion continua mientras haya contacto (mas simple de sondear). */
    bsp_i2c_write_reg(BOARD_TP_ADDR, 0xFA, 0x71);
    /* Desactiva el auto-reposo para que no se duerma jugando. */
    bsp_i2c_write_reg(BOARD_TP_ADDR, 0xFE, 0x01);

    uint8_t chip = 0;
    bsp_i2c_read_reg(BOARD_TP_ADDR, 0xA7, &chip, 1);
    ESP_LOGI(TAG, "tactil listo (chip id 0x%02X)", chip);
    return ESP_OK;
}

bool bsp_touch_read(int *x, int *y)
{
    uint8_t d[6];
    if (bsp_i2c_read_reg(BOARD_TP_ADDR, 0x01, d, sizeof(d)) != ESP_OK) return false;
    if ((d[1] & 0x0F) == 0) return false;   /* ningun dedo */

    int px = ((d[2] & 0x0F) << 8) | d[3];
    int py = ((d[4] & 0x0F) << 8) | d[5];
    if (px < 0 || px >= BOARD_LCD_WIDTH || py < 0 || py >= BOARD_LCD_HEIGHT) return false;

    if (x) *x = px;
    if (y) *y = py;
    return true;
}
