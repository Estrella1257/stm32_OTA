#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "main.h"
#include "usart.h"

volatile uint8_t g_rx_data = 0;
volatile uint8_t g_rx_flag = 0;

static void usart_received(uint8_t data)
{
    //printf("received: %c ascii: %d\r\n", data, data);
    g_rx_data = data;
    g_rx_flag = 1;
}

void LED_INIT(void) {
    led_init(&led1);
    led_init(&led2);
    led_init(&led3);
    // led_init(&led4);
    led_on(&led1);
    led_on(&led2);
    led_on(&led3);
    // led_on(&led4);
}

void LED_OFF(void) {
    led_off(&led1);
    led_off(&led2);
    led_off(&led3);
    // led_off(&led4);
}

int main(void)
{
    __enable_irq();
    SCB->VTOR = 0x08008000;

    board_lowlevel_init();
    usart_init();
    usart_receive_register(usart_received);

    printf("boot is ok!\r\n");
    LED_INIT();
    printf("led is on!\r\n");
    while (1) 
    {
        if (g_rx_flag == 1)
        {
            // 可以在这里尽情 printf，因为这里被打断也没事
            printf("Received: %c (Ascii: %d)\r\n", g_rx_data, g_rx_data);
            
            g_rx_flag = 0; // 清除标志
            
            LED_OFF();
        }
    };
}