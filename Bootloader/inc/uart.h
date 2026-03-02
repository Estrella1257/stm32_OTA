#ifndef __USART_H
#define __USART_H

#include <stdint.h>
#include <stdio.h>

// 初始化串口 (默认波特率 115200)
void UART_Init(void);

// 发送一个字符
void UART_SendChar(uint8_t ch);

#endif