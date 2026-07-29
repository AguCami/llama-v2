/*
 * Mapa de pines de la placa Waveshare ESP32-S3-Touch-LCD-1.83 (SKU 32790).
 *
 * Verificado contra pin_config.h y los ejemplos oficiales del repositorio
 * waveshareteam/ESP32-S3-Touch-LCD-1.83.
 *
 *   LCD    : ST7789 por SPI, 240 x 284, IPS
 *   Tactil : CST816T por I2C (0x15)
 *   IMU    : QMI8658 por I2C (0x6B)
 *   RTC    : PCF85063 por I2C (0x51)
 *   PMU    : AXP2101 por I2C (0x34)
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* ---- pantalla (SPI) ---- */
#define BOARD_LCD_HOST        SPI2_HOST
#define BOARD_LCD_PIN_DC      4
#define BOARD_LCD_PIN_CS      5
#define BOARD_LCD_PIN_SCK     6
#define BOARD_LCD_PIN_MOSI    7
#define BOARD_LCD_PIN_RST     38
#define BOARD_LCD_PIN_BL      40
#define BOARD_LCD_WIDTH       240
#define BOARD_LCD_HEIGHT      284
#define BOARD_LCD_SPI_HZ      (60 * 1000 * 1000)

/* ---- bus I2C compartido ---- */
#define BOARD_I2C_PORT        0
#define BOARD_I2C_PIN_SDA     15
#define BOARD_I2C_PIN_SCL     14
#define BOARD_I2C_HZ          400000

/* ---- tactil ---- */
#define BOARD_TP_ADDR         0x15
#define BOARD_TP_PIN_RST      39
#define BOARD_TP_PIN_INT      13

/* ---- sensores y energia ---- */
#define BOARD_IMU_ADDR_L      0x6B
#define BOARD_IMU_ADDR_H      0x6A
#define BOARD_RTC_ADDR        0x51
#define BOARD_PMU_ADDR        0x34

#endif /* BOARD_CONFIG_H */
