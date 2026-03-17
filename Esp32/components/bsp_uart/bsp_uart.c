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
#define MY_UART_BUF_SIZE      (1024 * 2)  

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

    uint8_t *data = (uint8_t *) malloc(MY_UART_BUF_SIZE);

    while (1) {
        int len = uart_read_bytes(MY_UART_PORT_NUM, data, MY_UART_BUF_SIZE, pdMS_TO_TICKS(100));
        if (len > 0) {
            // 把收到的数据扔给 protocol 组件去拆包
            parse_uart_buffer(data, len);
        }
    }
}

void bsp_uart_init(void) {
    xTaskCreate(uart_rx_task, "uart_rx_task", 1024 * 4, NULL, 10, NULL);
}