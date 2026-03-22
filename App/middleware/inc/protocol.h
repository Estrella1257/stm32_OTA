#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "ringbuffer.h" 
#include "usart.h"
#include "stm32f4xx.h" 
#include <stdio.h>

#define CMD_START_OTA 0x02

void Protocol_HardwareRx_Poll(ringbuffer_t *rb); 
void Protocol_Poll(ringbuffer_t *rb);

// void Parse_ESP32_Commands(ringbuffer_t *rb);
void Protocol_Send_Telemetry(int16_t speed, uint8_t battery, uint8_t gear);

#endif 