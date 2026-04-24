#ifndef MOTOR_H
#define MOTOR_H

#include "stm32f4xx.h"

// 暴露出初始化和设置占空比的 API
void Motor_PWM_Init(void);

// 传入 a, b, c 的占空比值 (范围：0 ~ 4200)
void Motor_Set_PWM(uint16_t pwm_a, uint16_t pwm_b, uint16_t pwm_c);

// 控制驱动板的使能开关
void Motor_Enable(void);
void Motor_Disable(void);

#endif