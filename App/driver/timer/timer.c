#include "stm32f4xx.h" 

volatile uint8_t g_imu_update_flag = 0;
volatile uint8_t g_ui_update_flag = 0; 

// 假设你的 APB1 定时器时钟为 84MHz (STM32F4 标准配置)
void tim3_init(void) 
{
    // 2. 配置 NVIC 中断优先级
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 4; // 优先级要比串口高一点
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 3. 配置定时器参数
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    // 预分频器：84000000 / 8400 = 10000 Hz (即定时器每 0.1 毫秒跳一下)
    TIM_TimeBaseStructure.TIM_Prescaler = 8400 - 1; 
    // 计数值：数 100 下就是 100 * 0.1ms = 10ms 触发一次中断
    TIM_TimeBaseStructure.TIM_Period = 100 - 1; 
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    // 4. 清除中断标志，使能更新中断，启动定时器
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

void TIM3_IRQHandler(void)
{
    // 检查是否是 TIM3 的更新中断
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) {
        
        static uint8_t tick_count = 0; // 静态局部变量，用来记录 10ms 心跳的次数
        tick_count++;

        // 1. 每 10ms 必定触发一次 IMU 姿态解算
        g_imu_update_flag = 1; 

        // 2. 每进 5 次 (5 * 10ms = 50ms)，触发一次 VCU 业务
        if (tick_count >= 5) {
            g_ui_update_flag = 1; 
            tick_count = 0; // 触发后清零，重新开始数
        }
        
        // 务必清除中断标志位，否则系统会卡死在这里
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    }
}

// 初始化 TIM6 为 1ms 绝对心跳
void TIM6_1ms_Init_And_Enable(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 1. 配置定时器基础参数 (假设 APB1 Timer Clock = 84MHz)
    // 预分频器设为 84-1，计数器时钟为 1MHz (即 1 微秒跳一次)
    TIM_TimeBaseStructure.TIM_Prescaler = 84 - 1;       
    // 计 1000 个微秒 = 1 毫秒
    TIM_TimeBaseStructure.TIM_Period = 1000 - 1;        
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM6, &TIM_TimeBaseStructure);

    // 2. 配置 NVIC 中断优先级 (极其关键！)
    // 注意：STM32F4 中 TIM6 的中断向量名和 DAC 绑在一起
    NVIC_InitStructure.NVIC_IRQChannel = TIM6_DAC_IRQn; 
    
    // 抢占优先级设为 0 (最高)，子优先级设为 0
    // 绝不允许被 UART、I2C 或 你的 TIM3 打断
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0; 
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 3. 清除标志位并使能中断与定时器
    TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
    TIM_ITConfig(TIM6, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM6, ENABLE);
}

// TIM6 硬件中断服务函数
void TIM6_DAC_IRQHandler(void)
{
    // 检查是否是 TIM6 的更新中断
    if (TIM_GetITStatus(TIM6, TIM_IT_Update) != RESET) {
        
        // 1. 呼叫你的 FOC 后台驻留任务 (1000Hz 极速执行)
        FOC_Hardware_1ms_ISR(); 

        // 2. 务必清除中断标志位
        TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
    }
}