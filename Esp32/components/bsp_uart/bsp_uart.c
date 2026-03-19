#include "bsp_uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"    
#include "driver/gpio.h"
#include "protocol.h" 

#define MY_UART_PORT_NUM      UART_NUM_0      
#define MY_UART_BAUD_RATE     115200          
#define MY_UART_TX_PIN        43             
#define MY_UART_RX_PIN        44             
#define MY_UART_BUF_SIZE      1024

extern volatile int16_t global_real_speed; 

static void uart_rx_task(void *arg)
{ 
    uart_config_t uart_config = {
        .baud_rate = MY_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(MY_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(MY_UART_PORT_NUM, MY_UART_TX_PIN, MY_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(MY_UART_PORT_NUM, MY_UART_BUF_SIZE, 0, 0, NULL, 0));

    // 魔法 1：只要硬件收到 9 个字节（刚好一个完整协议包），立刻中断叫醒 CPU！
    uart_set_rx_full_threshold(MY_UART_PORT_NUM, 9);
    
    // 魔法 2：如果线路空闲超过 3 个字节的时间没有新数据，也立刻叫醒 CPU（防丢包兜底）
    uart_set_rx_timeout(MY_UART_PORT_NUM, 3);

    uint8_t *data = (uint8_t *) malloc(MY_UART_BUF_SIZE);

    // while (1) {
    //     int len = uart_read_bytes(MY_UART_PORT_NUM, data, 128, pdMS_TO_TICKS(10));
    //     if (len > 0) {
    //         // 把收到的数据扔给 protocol 组件去拆包
    //         parse_uart_buffer(data, len);
    //     }
    //     vTaskDelay(1);
    // }

    while (1) {
        size_t length = 0;
        
        // 1. 探针：查询底层硬件缓冲区现在有多少数据（瞬间返回，绝不卡顿）
        ESP_ERROR_CHECK(uart_get_buffered_data_len(MY_UART_PORT_NUM, &length));
        
        if (length > 0) {
            // 2. 闪电读取：底层有多少，我们就读多少
            // 注意最后的参数 0！意思是“不准等，立刻把数据给我！”
            int len = uart_read_bytes(MY_UART_PORT_NUM, data, length, 0); 
            
            if (len > 0) {
                // 把收到的数据扔给 protocol 组件去拆包
                parse_uart_buffer(data, len);
                
                // 终极测谎仪：插个眼，看看到底是不是每 50 毫秒收到一次数据
                // (如果嫌打印太快卡顿，测试完可以把这句 printf 注释掉)
                printf("[UART] Time: %lu ms | Speed: %d\n", 
                      (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS), 
                      global_real_speed);
            }
        }
        
        // 3. 强制让出兵权：睡 1 个 Tick (大约 10 毫秒)。
        // 这一步极其重要，必须把 CPU 空出来给 LVGL 去画图
        vTaskDelay(1); 
    }
}

void bsp_uart_init(void) {
    xTaskCreate(uart_rx_task, "uart_rx_task", 1024 * 4, NULL, 10, NULL);
}