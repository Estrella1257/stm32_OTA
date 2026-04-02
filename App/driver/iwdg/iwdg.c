#include "iwdg.h"

// 初始化独立看门狗
// 超时时间计算：LSI时钟通常为 32kHz。
// 预分频设为 64，则看门狗时钟频率为 32000 / 64 = 500 Hz (即每跳一次需要 2ms)。
// 重装载值设为 500，则总超时时间 = 500 * 2ms = 1000ms (1秒)。
void IWDG_Init(void)
{
    // 1. 取消写保护：允许修改 IWDG 的 PR 和 RLR 寄存器
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

    // 2. 设置预分频器 (Prescaler) 为 64
    IWDG_SetPrescaler(IWDG_Prescaler_64);

    // 3. 设置重装载值 (Reload) 为 500
    // 范围是 0 ~ 0xFFF (即 0 ~ 4095)
    IWDG_SetReload(500);

    // 4. 重装载计数器 (喂一次狗，防止一开启就复位)
    IWDG_ReloadCounter();

    // 5. 正式使能看门狗 (一旦开启，直到断电重启前，都无法关闭)
    IWDG_Enable();
}

// 喂狗函数 (需要在主循环里定时调用)
void IWDG_Feed(void)
{
    // 把计数值重新刷回 500
    IWDG_ReloadCounter();
}