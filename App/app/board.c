#include "board.h"

void board_lowlevel_init(void)
{
	NVIC_SetPriorityGrouping(NVIC_PriorityGroup_4);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE); 
}

void System_Reset_State(void)
{
    __disable_irq();

    // 洗白 SysTick (系统滴答定时器)
    // 防止 FreeRTOS 或 HAL 库的延时函数被干扰
    SysTick->CTRL = 0; // 关闭 SysTick 定时器
    SysTick->LOAD = 0; // 清空重装载寄存器
    SysTick->VAL  = 0; // 清空当前计数值

    // 3. 洗白 NVIC (嵌套向量中断控制器)
    for (int i = 0; i < 8; i++) {
        // ICER (Interrupt Clear-Enable): 强制关闭所有已经开启的中断
        NVIC->ICER[i] = 0xFFFFFFFF; 
        
        // ICPR (Interrupt Clear-Pending): 强制清除所有正在排队、悬而未决的中断
        NVIC->ICPR[i] = 0xFFFFFFFF; 
    }

    // 4. 重定向中断向量表
    SCB->VTOR = 0x08010000;
    
    // 注意：这里不要调用 __enable_irq()
    // 全局中断必须等到你的 main 函数里所有的外设 (串口、DMA、I2C) 都初始化完毕后，再统一开启
}

const led_desc_t led1 =
{
    .clk_sourse = RCC_AHB1Periph_GPIOE,
    .port = GPIOE,
    .pin = GPIO_Pin_5,
    .on_lvl = Bit_RESET,
    .off_lvl = Bit_SET,
};

const led_desc_t led2 =
{
    .clk_sourse = RCC_AHB1Periph_GPIOE,
    .port = GPIOE,
    .pin = GPIO_Pin_6,
    .on_lvl = Bit_RESET,
    .off_lvl = Bit_SET,
};

const led_desc_t led3 =
{
    .clk_sourse = RCC_AHB1Periph_GPIOC,
    .port = GPIOC,
    .pin = GPIO_Pin_13,
    .on_lvl = Bit_RESET,
    .off_lvl = Bit_SET,
};