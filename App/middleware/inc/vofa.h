#ifndef __VOFA_H
#define __VOFA_H

#include "stm32f4xx.h"

// JustFloat 协议帧尾定义声明 (供内部或外部查阅)
extern const uint8_t VOFA_Tail[4];

// ================= 函数声明 =================
// 发送 JustFloat 数据帧
// data_array: 浮点数数组指针
// channel_num: 数组长度 (通道数)
void VOFA_JustFloat_Send(float *data_array, uint8_t channel_num);

#endif