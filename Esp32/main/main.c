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

// 全局神经枢纽
volatile int16_t global_real_speed = 0; 
volatile int16_t global_battery = 0; 
volatile int16_t global_gear = 0;
volatile int16_t global_odo = 1000; 

void app_main(void)
{
    printf("Dual-Core Communication System\n");

    // 1. 启动底层硬件驱动 
    bsp_uart_init();
    lv_disp_t *disp = bsp_lcd_init();

    // 2. 启动上层业务逻辑 
    // 初始化时用 portMAX_DELAY 死等，确保必须拿到锁
    lvgl_port_lock(portMAX_DELAY); 
    ui_init(disp);
    lvgl_port_unlock();

    protocol_service_start();

    // 3. 进入主循环 (20Hz 极速刷新) 
    int last_speed = -1;
    
    while (1) {
        int current_speed = global_real_speed;
        int current_battery = global_battery;
        int current_gear = global_gear;

        if (current_speed != last_speed) {
            if (lvgl_port_lock(portMAX_DELAY)) {
                ui_update_dashboard(current_speed, current_battery, current_gear, false, true, global_odo);
                lvgl_port_unlock();
            }
            last_speed = current_speed;
        }

        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}