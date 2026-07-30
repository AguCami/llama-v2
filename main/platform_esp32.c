/*
 * Implementacion de la capa de plataforma para el ESP32-S3:
 * hora del sistema (sincronizada con el PCF85063), guardado en NVS y sonido.
 */
#include "llama_platform.h"
#include "bsp.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include "nvs.h"
#include <time.h>
#include <sys/time.h>
#include <string.h>

static const char *TAG = "plat";

#define NVS_NS   "llama"
#define NVS_KEY  "pet"

double llama_plat_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + tv.tv_usec / 1e6;
}

uint32_t llama_plat_millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

bool llama_plat_save(const void *blob, size_t len)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_blob(h, NVS_KEY, blob, len);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) ESP_LOGW(TAG, "no se pudo guardar: %s", esp_err_to_name(err));
    return err == ESP_OK;
}

bool llama_plat_load(void *blob, size_t len)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t sz = len;
    esp_err_t err = nvs_get_blob(h, NVS_KEY, blob, &sz);
    nvs_close(h);
    return err == ESP_OK && sz == len;
}

/*
 * Sonido: la placa lleva un ES8311 + ES7210 sobre I2S. Los pines de audio no
 * estan publicados fuera del BSP oficial de Waveshare, asi que dejamos el
 * enganche listo y sin implementar en vez de adivinar el cableado.
 * Para activarlo: agregar el componente `waveshare/esp32_s3_touch_lcd_1_83`
 * e inicializar el codec aca, generando un seno de `freq_hz` durante `ms`.
 */
void llama_plat_tone(int freq_hz, int ms)
{
    (void)freq_hz;
    (void)ms;
}

void llama_plat_haptic(int ms)
{
    (void)ms;   /* la placa no trae motor vibrador */
}

uint32_t llama_plat_seed(void)
{
    return esp_random();
}

/*
 * La hoja de sprites se embute en el binario y queda mapeada en flash, asi que
 * se lee con un puntero y no gasta un byte de RAM.
 */
extern const uint8_t llama_sprites_start[] asm("_binary_llama_sprites_bin_start");
extern const uint8_t llama_sprites_end[]   asm("_binary_llama_sprites_bin_end");

const void *llama_plat_sprites(size_t *len)
{
    size_t n = (size_t)(llama_sprites_end - llama_sprites_start);
    if (len) *len = n;
    return n > 32 ? llama_sprites_start : NULL;
}
