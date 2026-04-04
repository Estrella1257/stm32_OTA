#ifndef __THROTTLE_H__
#define __THROTTLE_H__

#include "stm32f4xx.h"

// 定义 DMA 缓冲区大小 (采集 16 次求平均，滤波让油门极其丝滑)
#define ADC_FILTER_SIZE  16 

#define ADC_DEADZONE_MIN  1100  // 低于此值算作没拧油门
#define ADC_MAX_THROTTLE  3000  // 高于此值算作油门拧到底

void ADC_DMA_Init(void);
uint8_t Get_Percent(void);

#endif 