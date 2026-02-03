#include "boot.h"

int boot_check_app(uint32_t app_addr)
{
    uint32_t sp = *(volatile uint32_t *)app_addr;

    // STM32 RAM 地址范围：0x20000000 ~ 0x20020000 (F407)
    // 等效于if (sp >= 0x20000000 && sp <= 0x2001FFFF)
    if ((sp & 0x2FFE0000) == 0x20000000)
    {
        return 1; // 看起来是合法栈指针
    }
    return 0;
}
