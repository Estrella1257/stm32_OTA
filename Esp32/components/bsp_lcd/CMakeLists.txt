idf_component_register(SRCS "bsp_lcd.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_lcd lvgl esp_lvgl_port ui esp_lcd_ili9341)