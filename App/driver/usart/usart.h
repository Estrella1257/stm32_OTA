#ifndef __USART_H
#define __USART_H

#include "stm32f4xx.h"
#include <stdint.h>

typedef void (*usart_receive_callback_t)(uint8_t data);

void usart1_init(void);
void usart1_send_data(const char str[]);
void usart1_receive_register(usart_receive_callback_t callback);
void usart2_init(void);
void usart2_receive_register(usart_receive_callback_t callback);

#endif