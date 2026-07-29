/*
 * Mascota virtual 3D "Mi Llama" para Waveshare ESP32-S3-Touch-LCD-1.83.
 *
 * Arranque -> inicializa la placa, restaura la partida desde NVS, recupera el
 * tiempo transcurrido con el RTC y entra en el bucle de juego. Tras un rato sin
 * tocar la pantalla baja el brillo y finalmente entra en sueno profundo,
 * despertandose al primer toque (la llama sigue "viviendo" gracias al RTC).
 */
#include "bsp.h"
#include "board_config.h"
#include "llama_game.h"
#include "llama_platform.h"
#include "g3d.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "llama";

#define FB_W BOARD_LCD_WIDTH
#define FB_H BOARD_LCD_HEIGHT

#define BRIGHT_FULL       100
#define BRIGHT_DIM        18
#define DIM_AFTER_MS      25000
#define SLEEP_AFTER_MS    180000

static g3d_target s_fb;

static bool framebuffer_alloc(void)
{
    s_fb.w = FB_W;
    s_fb.h = FB_H;
    s_fb.color = heap_caps_malloc((size_t)FB_W * FB_H * sizeof(uint16_t),
                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_fb.depth = heap_caps_malloc((size_t)FB_W * FB_H * sizeof(float),
                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_fb.color || !s_fb.depth) {
        /* Sin PSRAM no entra: probamos igual en RAM interna por las dudas. */
        ESP_LOGW(TAG, "sin PSRAM disponible, intento en RAM interna");
        free(s_fb.color);
        free(s_fb.depth);
        s_fb.color = malloc((size_t)FB_W * FB_H * sizeof(uint16_t));
        s_fb.depth = malloc((size_t)FB_W * FB_H * sizeof(float));
    }
    return s_fb.color && s_fb.depth;
}

static void go_to_sleep(llama_game *game)
{
    ESP_LOGI(TAG, "sin actividad: entro en sueno profundo");
    llama_game_save(game);
    bsp_rtc_store_system();
    bsp_display_backlight(0);
    vTaskDelay(pdMS_TO_TICKS(40));

    /* El INT del tactil baja a 0 cuando se apoya un dedo. */
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BOARD_TP_PIN_INT, 0);
    esp_deep_sleep_start();
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(bsp_i2c_init());
    bsp_power_init();
    bsp_rtc_init();
    bsp_rtc_sync_system();
    ESP_ERROR_CHECK(bsp_display_init());
    bsp_touch_init();
    bsp_imu_init();

    if (!framebuffer_alloc()) {
        ESP_LOGE(TAG, "no hay memoria para el framebuffer");
        return;
    }
    ESP_LOGI(TAG, "framebuffer %dx%d (%u KB color + %u KB profundidad)",
             FB_W, FB_H, (unsigned)(FB_W * FB_H * 2 / 1024),
             (unsigned)(FB_W * FB_H * 4 / 1024));

    llama_game *game = llama_game_create(FB_W, FB_H);
    if (!game) {
        ESP_LOGE(TAG, "no se pudo crear el juego");
        return;
    }

    /* Encendido suave de la retroiluminacion. */
    for (int i = 0; i <= BRIGHT_FULL; i += 4) {
        bsp_display_backlight(i);
        vTaskDelay(pdMS_TO_TICKS(8));
    }

    int64_t last_us    = esp_timer_get_time();
    int64_t last_touch = last_us;
    int     brightness = BRIGHT_FULL;
    int     bat_pct    = -1;
    bool    charging   = false;
    int     bat_div    = 0;

    while (1) {
        const int64_t now_us = esp_timer_get_time();
        float dt = (float)(now_us - last_us) / 1e6f;
        last_us = now_us;
        if (dt <= 0.f) dt = 0.001f;

        llama_input in;
        memset(&in, 0, sizeof(in));

        int tx = 0, ty = 0;
        if (bsp_touch_read(&tx, &ty)) {
            in.touch_down = true;
            in.touch_x = tx;
            in.touch_y = ty;
            last_touch = now_us;
            if (brightness != BRIGHT_FULL) {
                brightness = BRIGHT_FULL;
                bsp_display_backlight(brightness);
            }
        }

        bsp_imu_read(&in.ax, &in.ay, &in.az, &in.gx, &in.gy, &in.gz);

        /* La bateria se lee una vez por segundo, no hace falta mas. */
        if (--bat_div <= 0) {
            bat_div  = 30;
            bat_pct  = bsp_power_battery_percent();
            charging = bsp_power_charging();
        }
        in.battery_pct = bat_pct;
        in.charging    = charging;

        llama_game_frame(game, &in, dt, &s_fb);
        bsp_display_flush(s_fb.color);

        /* Ahorro de energia. */
        const int64_t idle_ms = (now_us - last_touch) / 1000;
        if (idle_ms > DIM_AFTER_MS && brightness != BRIGHT_DIM && !llama_game_busy(game)) {
            brightness = BRIGHT_DIM;
            bsp_display_backlight(brightness);
        }
        if (idle_ms > SLEEP_AFTER_MS && !llama_game_busy(game)) {
            go_to_sleep(game);
        }

        /* Limita a ~30 fps para no fundir la bateria. */
        const int64_t frame_us = esp_timer_get_time() - now_us;
        if (frame_us < 33000) {
            vTaskDelay(pdMS_TO_TICKS((33000 - frame_us) / 1000));
        } else {
            vTaskDelay(1);
        }
    }
}
