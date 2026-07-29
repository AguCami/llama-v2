/*
 * Pantalla ST7789 (240x284) por SPI + retroiluminacion por PWM.
 *
 * El framebuffer del juego vive en PSRAM en formato RGB565 little endian.
 * El ST7789 espera big endian, asi que el volcado copia por franjas a un
 * buffer interno apto para DMA invirtiendo los bytes.
 */
#include "bsp.h"
#include "board_config.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "bsp_lcd";

#define BAND_LINES 40
#define BAND_PIX   (BOARD_LCD_WIDTH * BAND_LINES)

static esp_lcd_panel_handle_t    s_panel;
static esp_lcd_panel_io_handle_t s_io;
static uint16_t                 *s_band[2];
static int                       s_band_idx;
static SemaphoreHandle_t         s_flush_done;

static bool on_color_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *ev,
                          void *ctx)
{
    BaseType_t hp = pdFALSE;
    xSemaphoreGiveFromISR(s_flush_done, &hp);
    return hp == pdTRUE;
}

static void backlight_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 20000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .gpio_num   = BOARD_LCD_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&ch);
}

void bsp_display_backlight(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    /* Curva suave: el ojo percibe mejor una rampa cuadratica. */
    uint32_t duty = (uint32_t)((percent * percent * 1023) / 10000);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

esp_err_t bsp_display_init(void)
{
    backlight_init();

    s_flush_done = xSemaphoreCreateCounting(4, 0);
    if (!s_flush_done) return ESP_ERR_NO_MEM;

    spi_bus_config_t bus = {
        .sclk_io_num     = BOARD_LCD_PIN_SCK,
        .mosi_io_num     = BOARD_LCD_PIN_MOSI,
        .miso_io_num     = -1,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = BAND_PIX * 2 + 64,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(BOARD_LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = BOARD_LCD_PIN_DC,
        .cs_gpio_num       = BOARD_LCD_PIN_CS,
        .pclk_hz           = BOARD_LCD_SPI_HZ,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 4,
        .on_color_trans_done = on_color_done,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_HOST,
                                             &io_cfg, &s_io));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BOARD_LCD_PIN_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_io, &panel_cfg, &s_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));   /* panel IPS */
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    for (int i = 0; i < 2; i++) {
        s_band[i] = heap_caps_malloc(BAND_PIX * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (!s_band[i]) {
            ESP_LOGE(TAG, "sin memoria para el buffer de volcado");
            return ESP_ERR_NO_MEM;
        }
    }
    ESP_LOGI(TAG, "pantalla lista (%dx%d)", BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
    return ESP_OK;
}

esp_err_t bsp_display_flush(const uint16_t *fb)
{
    if (!s_panel) return ESP_ERR_INVALID_STATE;

    int pending = 0;
    for (int y = 0; y < BOARD_LCD_HEIGHT; y += BAND_LINES) {
        int lines = BOARD_LCD_HEIGHT - y;
        if (lines > BAND_LINES) lines = BAND_LINES;
        const int n = lines * BOARD_LCD_WIDTH;

        /* Espera a que el buffer que vamos a reusar este libre. */
        if (pending >= 2) {
            xSemaphoreTake(s_flush_done, portMAX_DELAY);
            pending--;
        }
        uint16_t *dst = s_band[s_band_idx];
        const uint16_t *src = fb + (size_t)y * BOARD_LCD_WIDTH;
        for (int i = 0; i < n; i++) {
            uint16_t c = src[i];
            dst[i] = (uint16_t)((c >> 8) | (c << 8));
        }
        s_band_idx ^= 1;

        esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, y, BOARD_LCD_WIDTH, y + lines, dst);
        if (err != ESP_OK) return err;
        pending++;
    }
    while (pending > 0) {
        xSemaphoreTake(s_flush_done, portMAX_DELAY);
        pending--;
    }
    return ESP_OK;
}
