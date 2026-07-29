/*
 * Gestion de energia (AXP2101) y reloj de tiempo real (PCF85063).
 *
 * Del AXP2101 solo usamos el medidor de bateria y el estado de carga; la placa
 * ya arranca con los rieles configurados por el cargador de la propia PMU.
 */
#include "bsp.h"
#include "board_config.h"
#include "esp_log.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG = "bsp_power";

/* ------------------------------------------------------------------ PMU */

#define AXP_STATUS1     0x00
#define AXP_STATUS2     0x01
#define AXP_ADC_ENABLE  0x30
#define AXP_BAT_PERCENT 0xA4

static bool s_pmu_ok;

esp_err_t bsp_power_init(void)
{
    uint8_t st = 0;
    if (bsp_i2c_read_reg(BOARD_PMU_ADDR, AXP_STATUS1, &st, 1) != ESP_OK) {
        ESP_LOGW(TAG, "AXP2101 no responde");
        return ESP_ERR_NOT_FOUND;
    }
    uint8_t adc = 0;
    bsp_i2c_read_reg(BOARD_PMU_ADDR, AXP_ADC_ENABLE, &adc, 1);
    bsp_i2c_write_reg(BOARD_PMU_ADDR, AXP_ADC_ENABLE, adc | 0x01); /* ADC de bateria */
    s_pmu_ok = true;
    ESP_LOGI(TAG, "PMU lista (status 0x%02X)", st);
    return ESP_OK;
}

int bsp_power_battery_percent(void)
{
    if (!s_pmu_ok) return -1;
    uint8_t pct = 0;
    if (bsp_i2c_read_reg(BOARD_PMU_ADDR, AXP_BAT_PERCENT, &pct, 1) != ESP_OK) return -1;
    if (pct > 100) return -1;
    return pct;
}

bool bsp_power_charging(void)
{
    if (!s_pmu_ok) return false;
    uint8_t st = 0;
    if (bsp_i2c_read_reg(BOARD_PMU_ADDR, AXP_STATUS2, &st, 1) != ESP_OK) return false;
    return ((st >> 5) & 0x03) == 0x01;   /* 01 = cargando */
}

/* ------------------------------------------------------------------ RTC */

#define PCF_CTRL1   0x00
#define PCF_SECONDS 0x04

static uint8_t bcd2dec(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t dec2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

static bool s_rtc_ok;

esp_err_t bsp_rtc_init(void)
{
    uint8_t ctrl = 0;
    if (bsp_i2c_read_reg(BOARD_RTC_ADDR, PCF_CTRL1, &ctrl, 1) != ESP_OK) {
        ESP_LOGW(TAG, "PCF85063 no responde");
        return ESP_ERR_NOT_FOUND;
    }
    /* Limpia STOP para que el reloj corra. */
    bsp_i2c_write_reg(BOARD_RTC_ADDR, PCF_CTRL1, ctrl & ~0x20);
    s_rtc_ok = true;
    return ESP_OK;
}

bool bsp_rtc_get_time(struct tm *out)
{
    if (!s_rtc_ok || !out) return false;
    uint8_t d[7];
    if (bsp_i2c_read_reg(BOARD_RTC_ADDR, PCF_SECONDS, d, sizeof(d)) != ESP_OK) return false;
    if (d[0] & 0x80) return false;   /* oscilador detenido: hora no confiable */

    memset(out, 0, sizeof(*out));
    out->tm_sec  = bcd2dec(d[0] & 0x7F);
    out->tm_min  = bcd2dec(d[1] & 0x7F);
    out->tm_hour = bcd2dec(d[2] & 0x3F);
    out->tm_mday = bcd2dec(d[3] & 0x3F);
    out->tm_wday = d[4] & 0x07;
    out->tm_mon  = bcd2dec(d[5] & 0x1F) - 1;
    out->tm_year = bcd2dec(d[6]) + 100;   /* 20xx */
    return true;
}

bool bsp_rtc_set_time(const struct tm *in)
{
    if (!s_rtc_ok || !in) return false;
    uint8_t buf[8];
    buf[0] = PCF_SECONDS;
    buf[1] = dec2bcd((uint8_t)in->tm_sec) & 0x7F;
    buf[2] = dec2bcd((uint8_t)in->tm_min);
    buf[3] = dec2bcd((uint8_t)in->tm_hour);
    buf[4] = dec2bcd((uint8_t)in->tm_mday);
    buf[5] = (uint8_t)(in->tm_wday & 0x07);
    buf[6] = dec2bcd((uint8_t)(in->tm_mon + 1));
    buf[7] = dec2bcd((uint8_t)(in->tm_year % 100));
    return bsp_i2c_write(BOARD_RTC_ADDR, buf, sizeof(buf)) == ESP_OK;
}

/* Dias desde el 1970-01-01 (algoritmo de Howard Hinnant). */
static long days_from_civil(long y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}

void bsp_rtc_sync_system(void)
{
    struct tm t;
    if (!bsp_rtc_get_time(&t)) {
        ESP_LOGW(TAG, "sin hora valida en el RTC; arranco desde el reloj interno");
        return;
    }
    long days = days_from_civil(t.tm_year + 1900, (unsigned)(t.tm_mon + 1),
                                (unsigned)t.tm_mday);
    time_t epoch = (time_t)(days * 86400L + t.tm_hour * 3600L + t.tm_min * 60L + t.tm_sec);
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "hora del RTC: %04d-%02d-%02d %02d:%02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
}

void bsp_rtc_store_system(void)
{
    time_t now = time(NULL);
    struct tm t;
    gmtime_r(&now, &t);
    bsp_rtc_set_time(&t);
}
