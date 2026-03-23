idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS ""
                       REQUIRES driver bsp_lcd bsp_uart esp_lvgl_port ui protocol bsp_wifi nvs_flash bsp_http_ota)
