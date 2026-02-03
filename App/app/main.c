#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "main.h"
#include "usart.h"


static void usart_received(uint8_t data)
{
    printf("received: %c ascii: %d\r\n", data, data);
}

void LED_INIT(void) {
    led_init(&led1);
    led_init(&led2);
    led_init(&led3);
    led_init(&led4);
    led_on(&led1);
    led_on(&led2);
    led_on(&led3);
    led_on(&led4);
}

int main(void)
{
    SCB->VTOR = 0x08008000;
    board_lowlevel_init();
    usart_init();
    usart_receive_register(usart_received);

    printf("boot is ok!\n");
    LED_INIT();
    printf("led is on!\n");
    while (1) 
    {

    };
}