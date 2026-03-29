#include "delay.h"

// 初始化 DWT 计数器
void DWT_Delay_Init(void)
{
    // 1. 使能 DWT 外设的时钟
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    
    // 2. 清零计数器
    DWT->CYCCNT = 0;
    
    // 3. 开启计数器
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

// 极其精准的微秒级延时
void delay_us(uint32_t us)
{
    uint32_t start_tick = DWT->CYCCNT;
    // SystemCoreClock 通常是 168000000，除以 1M 就是 1us 需要的时钟周期数 (168)
    uint32_t delay_ticks = us * (SystemCoreClock / 1000000); 
    
    // 死等，直到经过了足够的时钟周期
    // (因为是 32 位无符号整数减法，所以即使计数器溢出归零，这个减法也是绝对安全的)
    while ((DWT->CYCCNT - start_tick) < delay_ticks);
}

// 顺手写个毫秒延时 (可选)
void delay_ms(uint32_t ms)
{
    while (ms--) {
        delay_us(1000);
    }
}