#include "boot.h"
#include <stdio.h>

int main(void)
{
    // 最小初始化（GPIO / 串口 可选）
    // printf("Bootloader start...\r\n");

    if (boot_check_app(APP_START_ADDR))
    {
        // printf("Jumping to App...\r\n");
        boot_jump_to_app(APP_START_ADDR);
    }
    else
    {
        // printf("No valid app.\r\n");
    }

    while (1)
    {
        // 死循环（将来可进升级模式）
    }
}
