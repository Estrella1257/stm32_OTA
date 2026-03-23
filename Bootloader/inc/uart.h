#ifndef __UART_H
#define __UART_H

#include <stdint.h>
#include <stdio.h>

extern uint8_t ymodem_rx_buffer[];
extern volatile uint16_t ymodem_rx_len;
extern volatile uint8_t  ymodem_rx_flag;

void UART1_Log_Init(void);
void UART2_YModem_Init(void);
void UART2_SendChar(uint8_t ch);

#endif