#include "encoder.h"

// MT6701 默认 PPR 为 1024，开启定时器 4倍频 后一圈计数为 4096
#define ENCODER_CPR  4096.0f 

static int32_t last_counter = 0;
int32_t delta_pulses = 0;

void Encoder_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    
    // 1. 配置 PB6(A相), PB7(B相) 为复用功能
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;      
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;      
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 2. 引脚复用映射到 TIM4
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_TIM4);

    // 3. 定时器基础配置 (TIM4 是 16位，最大值 0xFFFF)
    TIM_TimeBaseStructure.TIM_Prescaler = 0;          
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;    
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    // 4. 开启编码器模式 3 (TI1 和 TI2 双边沿捕获)
    TIM_EncoderInterfaceConfig(TIM4, TIM_EncoderMode_TI12, 
                               TIM_ICPolarity_Rising, 
                               TIM_ICPolarity_Rising);

    // 5. 输入滤波 (滤除电机高频磁场干扰)
    TIM_ICInitTypeDef TIM_ICInitStructure;
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_ICFilter = 0x0F; 
    TIM_ICInit(TIM4, &TIM_ICInitStructure);

    TIM_SetCounter(TIM4, 0);
    TIM_Cmd(TIM4, ENABLE);
}

int32_t Encoder_Get_RawCount(void)
{
    // 获取 16 位的原始值 (0 ~ 65535)
    return (int32_t)TIM_GetCounter(TIM4);
}

// 必须严格放在 10ms 定时器中断里调用
float Encoder_Get_Velocity_RPM(float dt)
{
    int32_t current_counter = Encoder_Get_RawCount();
    
    // 算出两次采样的脉冲差值
    delta_pulses = current_counter - last_counter;
    
    // 【核心】16位溢出翻转处理
    // 如果 10ms 内差值竟然大于半圈(32767)，说明定时器偷偷归零溢出了，必须数学补偿回来
    if (delta_pulses > 32767) {
        delta_pulses -= 65536; // 向前疯狂溢出
    } else if (delta_pulses < -32768) {
        delta_pulses += 65536; // 向后疯狂溢出
    }
    
    last_counter = current_counter;

   // RPM 动态计算：(差值 / 一圈总脉冲数) * (1秒 / dt) * 60秒
    float rpm = ((float)delta_pulses / ENCODER_CPR) * (1.0f / dt) * 60.0f;
    return rpm;
}