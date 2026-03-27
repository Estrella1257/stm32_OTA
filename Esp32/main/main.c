#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_uart.h"   
#include "bsp_lcd.h"    
#include "ui.h"         
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "driver/uart.h"
#include "protocol.h"
#include "nvs_flash.h" 
#include "bsp_wifi.h"  
#include "bsp_http_ota.h"

// 全局神经枢纽
volatile int16_t global_real_speed = 0; 
volatile int16_t global_battery = 0; 
volatile int16_t global_gear = 0;
volatile int16_t global_odo = 1000; 
extern volatile bool g_wifi_connected;

void app_main(void)
{
    printf("ESP32 VCU GATEWAY V1.0\n");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        printf("[NVS] WRN -> Partition corrupted. Erasing and retrying...\n");
        // 如果 NVS 分区被破坏了，强制擦除再初始化
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    printf("[NVS] OK -> Flash partition initialized.\n");

    if (!bsp_spiffs_init()) return; 
    printf("[SYS] BOOT -> Loading drivers: UART, WiFi, LCD...\n");

    // 1. 启动底层硬件驱动 
    bsp_uart_init();
    bsp_wifi_init();
    lv_disp_t *disp = bsp_lcd_init();

    // 2. 启动上层业务逻辑 
    // 初始化时用 portMAX_DELAY 死等，确保必须拿到锁
    lvgl_port_lock(portMAX_DELAY); 
    ui_init(disp);
    lvgl_port_unlock();

    printf("[SYS] BOOT -> Starting Protocol Services...\n");
    protocol_service_start();

    printf("[SYS] OK -> All services online. Entering main loop.\n");

    // 3. 进入主循环 (20Hz 极速刷新) 
    int last_speed = -1;
    bool last_wifi = false;
    
    while (1) {
        int current_speed = global_real_speed;
        int current_battery = global_battery;
        int current_gear = global_gear;
        bool current_wifi = g_wifi_connected;

        if (current_speed != last_speed || current_wifi != last_wifi) {
            if (lvgl_port_lock(portMAX_DELAY)) {
                ui_update_dashboard(current_speed, current_battery, current_gear, current_wifi, true, global_odo);
                lvgl_port_unlock();
            }
            last_speed = current_speed;
            last_wifi = current_wifi;
        }

        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}