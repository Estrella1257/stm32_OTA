#include "stm32f4xx.h"

// 系统主频是 168MHz (STM32F407 的默认满血主频)
#define SYSTEM_CORE_CLOCK 168000000 

static volatile uint32_t g_sys_tick = 0;

// 初始化 SysTick 定时器，使其每 1ms 产生一次中断
void SysTick_Init(void)
{
    // 1. 设置重装载值 (LOAD)
    // 意思是：每经过多少个时钟周期产生一次中断？
    // 1秒 = 168,000,000 个周期，那 1毫秒 = 168,000 个周期。
    // 因为是从 0 开始数的，所以要减去 1。
    SysTick->LOAD = (SYSTEM_CORE_CLOCK / 1000) - 1;

    // 2. 清空当前计数值寄存器 (VAL)
    // 随便写个值进去，硬件会自动把它清零，并清除内部的标志位
    SysTick->VAL = 0;

    // 3. 配置控制寄存器 (CTRL) 并启动
    //SysTick->CTRL = (1UL << 2) | (1UL << 1) | (1UL << 0);
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |  // Bit 2: 使用处理器的主时钟 HCLK
                    SysTick_CTRL_TICKINT_Msk   |  // Bit 1: 倒数到 0 时，触发中断
                    SysTick_CTRL_ENABLE_Msk;      // Bit 0: 开启定时器
}

uint32_t Get_System_Tick(void)
{
    return g_sys_tick;
}

void SysTick_Handler(void)
{
    g_sys_tick++;
}