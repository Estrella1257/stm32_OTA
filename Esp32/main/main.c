#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_uart.h"   
#include "bsp_lcd.h"    
#include "ui.h"         
#include "lvgl.h"
#include "esp_lvgl_port.h"

// 全局神经枢纽
volatile int16_t global_real_speed = 0; 
volatile int16_t global_battery = 0; 
volatile int16_t global_odo = 1000; 

void app_main(void)
{
    printf("Dual-Core Communication System\n");

    // 1. 启动底层硬件驱动 
    bsp_uart_init();
    lv_disp_t *disp = bsp_lcd_init();

    // 2. 启动上层业务逻辑 (开张营业) 
    lvgl_port_lock(0);
    ui_init(disp);
    lvgl_port_unlock();

    // 3. 进入主循环 (坐镇指挥) 
    while (1) {
        int current_speed = global_real_speed;
        int current_battery = global_battery;
        int current_odo = global_odo;

        lvgl_port_lock(0);
        ui_update_dashboard(current_speed, current_battery, 1, false, true, current_odo);
        lvgl_port_unlock();

        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}