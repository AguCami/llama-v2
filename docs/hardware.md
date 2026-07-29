# Hardware: Waveshare ESP32-S3-Touch-LCD-1.83 (SKU 32790)

Datos tomados del repositorio oficial de la placa
(`waveshareteam/ESP32-S3-Touch-LCD-1.83`), archivo
`examples/arduino/libraries/Mylibrary/pin_config.h` y los ejemplos de ESP-IDF.

## Módulo

| Parámetro | Valor |
|---|---|
| MCU | ESP32-S3R8, doble núcleo LX7 a 240 MHz |
| PSRAM | 8 MB octal |
| Flash | 16 MB |
| Radio | Wi-Fi 2.4 GHz + BLE 5, antena en placa |
| Pantalla | 1.83" IPS 240 × 284, 65 K colores, ST7789 |
| Táctil | CST816T capacitivo |
| IMU | QMI8658 (acelerómetro + giróscopo) |
| RTC | PCF85063 con respaldo desde la PMU |
| Energía | AXP2101 (carga y gestión de batería Li-Po 3.7 V) |
| Audio | ES8311 (códec) + ES7210 (ADC con cancelación de eco) |
| Almacenamiento | ranura MicroSD en la parte trasera |
| Puerto | USB-C (carga, programación y logs) |

## Pines usados por este proyecto

### Pantalla (SPI2)

| Señal | GPIO |
|---|---|
| DC | 4 |
| CS | 5 |
| SCK | 6 |
| MOSI | 7 |
| RST | 38 |
| Retroiluminación | 40 (PWM por LEDC) |

Reloj SPI configurado a 60 MHz. El framebuffer se manda por franjas de 40 líneas
con doble buffer DMA en RAM interna, invirtiendo los bytes (el ESP32 es little
endian y el ST7789 espera big endian).

### Bus I2C compartido

| Señal | GPIO |
|---|---|
| SDA | 15 |
| SCL | 14 |

| Dispositivo | Dirección |
|---|---|
| CST816T (táctil) | 0x15 |
| AXP2101 (PMU) | 0x34 |
| PCF85063 (RTC) | 0x51 |
| QMI8658 (IMU) | 0x6B (alternativa 0x6A) |

### Táctil

| Señal | GPIO |
|---|---|
| RST | 39 |
| INT | 13 |

El INT también sirve para despertar del sueño profundo (`ext0`, nivel bajo).

## Registros que usa el firmware

### CST816T
- `0x01` GestureID, `0x02` cantidad de dedos
- `0x03`/`0x04` X (nibble alto + byte bajo), `0x05`/`0x06` Y
- `0xA7` chip id, `0xFA` control de interrupción, `0xFE` auto-reposo

### QMI8658
- `0x00` WHO_AM_I (debe leer 0x05)
- `0x02` CTRL1 = 0x40 (auto incremento de dirección)
- `0x03` CTRL2 = 0x15 (± 4 g, 117 Hz) → 8192 LSB/g
- `0x04` CTRL3 = 0x55 (± 512 dps, 117 Hz) → 64 LSB/dps
- `0x08` CTRL7 = 0x03 (habilita acelerómetro y giróscopo)
- `0x35`… 12 bytes con ax, ay, az, gx, gy, gz en int16 little endian

### PCF85063
- `0x00` Control_1 (se limpia el bit STOP al arrancar)
- `0x04`… 7 bytes BCD: segundos (bit 7 = oscilador detenido), minutos, horas,
  día, día de semana, mes, año

### AXP2101
- `0x00` estado 1, `0x01` estado 2 (bits 6:5 = estado de carga)
- `0x30` habilitación de ADC (bit 0 = batería)
- `0xA4` porcentaje de batería del medidor interno

## Consumo y batería

La placa viene con una batería Li-Po de 3.7 V con conector de 1.2 mm. El
firmware baja el brillo a los 25 segundos sin toques y entra en sueño profundo a
los 3 minutos; al despertar vuelve a leer el RTC y le "descuenta" a la llama todo
el tiempo que estuvo dormida (hasta 7 días).
