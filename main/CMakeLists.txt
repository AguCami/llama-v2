idf_component_register(
    SRCS "main.c"
         "platform_esp32.c"
         "bsp_i2c.c"
         "bsp_display.c"
         "bsp_touch.c"
         "bsp_imu.c"
         "bsp_power.c"
    INCLUDE_DIRS "."
    EMBED_FILES "../assets/llama_sprites.bin"
    REQUIRES g3d llamapet driver esp_lcd esp_timer nvs_flash esp_driver_gpio
             esp_driver_spi esp_driver_ledc esp_driver_i2c esp_hw_support)
