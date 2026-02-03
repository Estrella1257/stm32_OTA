#include "boot.h"
#include "stm32f4xx.h"

void boot_jump_to_app(uint32_t app_addr)
{
    uint32_t app_sp  = *(volatile uint32_t *)app_addr;
    uint32_t app_pc  = *(volatile uint32_t *)(app_addr + 4);

    app_entry_t app_entry = (app_entry_t)app_pc;

    __disable_irq();        // 关中断

    SysTick->CTRL = 0;     // 关 SysTick
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    SCB->VTOR = app_addr; // 重定向中断向量表

    __set_MSP(app_sp);    // 设置主栈指针

    app_entry();          // 跳转，不再返回
}
