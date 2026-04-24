#ifndef FOC_H
#define FOC_H

#include "stm32f4xx.h"

// 2804 常见极对数为 7（极数为 14，极对数就是 14/2 = 7）
#define POLE_PAIRS 7.0f

// 暴露出 SVPWM 生成函数和开环测试函数
void FOC_Set_Phase_Voltage(float Uq, float angle_el);
void FOC_Align_Sensor(void);
void FOC_ClosedLoop_Voltage_Test(float target_voltage);
float LowPass_Filter(float new_val, float prev_val, float Tf, float dt);

#endif