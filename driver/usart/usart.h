#ifndef __USART_H
#define __USART_H

#include "stm32f4xx.h"
#include <stdint.h>

typedef void (*usart_receive_callback_t)(uint8_t data);

void usart_init(void);
void usart_send_data(const char str[]);
void usart_receive_register(usart_receive_callback_t callback);

#endif