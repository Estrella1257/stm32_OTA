#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "ringbuffer.h" 
#include "usart.h"
#include "stm32f4xx.h" 
#include <stdio.h>

#define CMD_REPORT_STATE   0x01
#define CMD_STM32_ALIVE    0x02
#define CMD_ENTER_BOOT     0x03

void Protocol_HardwareRx_Poll(ringbuffer_t *rb); 
void Protocol_Poll(ringbuffer_t *rb);
void Protocol_Send_Telemetry(int16_t speed, uint8_t battery, uint8_t gear);
void Protocol_Send_Alive_Ping(void);

#endif 