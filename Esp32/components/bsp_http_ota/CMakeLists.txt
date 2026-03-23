idf_component_register(SRCS "bsp_http_ota.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_http_client spiffs bsp_http_ota)