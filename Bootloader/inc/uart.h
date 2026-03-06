#ifndef __UART_H
#define __UART_H

#include <stdint.h>
#include <stdio.h>

extern uint8_t ymodem_rx_buffer[];
extern volatile uint16_t ymodem_rx_len;
extern volatile uint8_t  ymodem_rx_flag;

// 初始化串口 (默认波特率 115200)
void UART_Init(void);

// 发送一个字符
void UART_SendChar(uint8_t ch);

#endif