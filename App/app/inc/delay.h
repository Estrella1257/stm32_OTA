#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f4xx.h"

// 函数声明
void DWT_Delay_Init(void);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

#endif