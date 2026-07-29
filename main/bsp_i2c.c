/*
 * Bus I2C compartido por el tactil, el IMU, el RTC y el PMU.
 * Usa la API nueva (driver/i2c_master.h) de ESP-IDF 5.x.
 */
#include "bsp.h"
#include "board_config.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "bsp_i2c";

static i2c_master_bus_handle_t s_bus;

/* Cache de handles por direccion para no recrearlos en cada acceso. */
#define MAX_DEVS 6
static struct {
    uint8_t addr;
    i2c_master_dev_handle_t dev;
} s_devs[MAX_DEVS];
static int s_dev_count;

static i2c_master_dev_handle_t dev_for(uint8_t addr)
{
    for (int i = 0; i < s_dev_count; i++) {
        if (s_devs[i].addr == addr) return s_devs[i].dev;
    }
    if (s_dev_count >= MAX_DEVS) return NULL;

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = BOARD_I2C_HZ,
    };
    i2c_master_dev_handle_t dev = NULL;
    if (i2c_master_bus_add_device(s_bus, &cfg, &dev) != ESP_OK) return NULL;
    s_devs[s_dev_count].addr = addr;
    s_devs[s_dev_count].dev  = dev;
    s_dev_count++;
    return dev;
}

esp_err_t bsp_i2c_init(void)
{
    if (s_bus) return ESP_OK;
    i2c_master_bus_config_t cfg = {
        .i2c_port          = BOARD_I2C_PORT,
        .sda_io_num        = BOARD_I2C_PIN_SDA,
        .scl_io_num        = BOARD_I2C_PIN_SCL,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&cfg, &s_bus);
    if (err != ESP_OK) ESP_LOGE(TAG, "i2c_new_master_bus: %s", esp_err_to_name(err));
    return err;
}

esp_err_t bsp_i2c_write(uint8_t addr, const uint8_t *data, size_t len)
{
    i2c_master_dev_handle_t dev = dev_for(addr);
    if (!dev) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit(dev, data, len, 100);
}

esp_err_t bsp_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value)
{
    const uint8_t buf[2] = { reg, value };
    return bsp_i2c_write(addr, buf, 2);
}

esp_err_t bsp_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    i2c_master_dev_handle_t dev = dev_for(addr);
    if (!dev) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, 100);
}

bool bsp_i2c_probe(uint8_t addr)
{
    return s_bus && i2c_master_probe(s_bus, addr, 50) == ESP_OK;
}
