#ifndef __USART_H
#define __USART_H

#include "stm32f4xx.h"
#include <stdint.h>

typedef void (*usart_receive_callback_t)(uint8_t data);

void usart1_init(void);
void usart1_send_data(const char str[]);
void usart1_receive_register(usart_receive_callback_t callback);
void usart2_init_dma(void);
uint16_t usart2_get_buffered_data_len(void);
uint16_t usart2_read_bytes(uint8_t *buf, uint16_t max_len);
void usart2_send_data_dma(const uint8_t *data, uint16_t len);
void usart3_init(void);

#endif