#include "motor.h"

// FOC 设定的周期极值：对应 20kHz 频率
#define PWM_PERIOD 4200 

void Motor_PWM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    // 1. 配置 PC6(U), PC7(V), PC8(W) 为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // 配置 PC9 为普通推挽输出，用于控制驱动板的 EN 使能引脚
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    Motor_Disable(); // 默认开机关闭电机，防暴冲！

    // 2. 引脚复用映射到 TIM8
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_TIM8);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_TIM8);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource8, GPIO_AF_TIM8);

    // 3. 定时器基础配置：极其重要的中央对齐模式
    TIM_TimeBaseStructure.TIM_Prescaler = 0; // 不分频，满血 168MHz
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_CenterAligned1; // 中央对齐模式1
    TIM_TimeBaseStructure.TIM_Period = PWM_PERIOD - 1; 
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM8, &TIM_TimeBaseStructure);

    // 4. 配置 3 个通道的 PWM 输出模式
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1; 
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable; // 咱们没用到互补输出引脚
    TIM_OCInitStructure.TIM_Pulse = 0; // 初始占空比为 0
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCIdleState_Reset;

    TIM_OC1Init(TIM8, &TIM_OCInitStructure); // 通道1 (PC6)
    TIM_OC1PreloadConfig(TIM8, TIM_OCPreload_Enable);

    TIM_OC2Init(TIM8, &TIM_OCInitStructure); // 通道2 (PC7)
    TIM_OC2PreloadConfig(TIM8, TIM_OCPreload_Enable);

    TIM_OC3Init(TIM8, &TIM_OCInitStructure); // 通道3 (PC8)
    TIM_OC3PreloadConfig(TIM8, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM8, ENABLE);

    // 5. 【极度关键】高级定时器专属：必须开启主输出使能 (MOE)，否则连个毛线波形都没有！
    TIM_CtrlPWMOutputs(TIM8, ENABLE);

    // 6. 启动定时器
    TIM_Cmd(TIM8, ENABLE);
}

// 动态设置 3 相的占空比 (val范围：0 ~ 4200)
void Motor_Set_PWM(uint16_t pwm_u, uint16_t pwm_v, uint16_t pwm_w)
{
    // 安全限幅保护，防止溢出炸管
    if(pwm_u > PWM_PERIOD) pwm_u = PWM_PERIOD;
    if(pwm_v > PWM_PERIOD) pwm_v = PWM_PERIOD;
    if(pwm_w > PWM_PERIOD) pwm_w = PWM_PERIOD;

    TIM8->CCR1 = pwm_u;
    TIM8->CCR2 = pwm_v;
    TIM8->CCR3 = pwm_w;
}

void Motor_Enable(void)  { GPIO_SetBits(GPIOC, GPIO_Pin_9);   }
void Motor_Disable(void) { GPIO_ResetBits(GPIOC, GPIO_Pin_9); }