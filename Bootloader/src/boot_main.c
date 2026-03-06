#include "boot.h"
#include "ota_config.h"
#include <stdio.h>
#include "uart.h"
#include "hal_flash.h"

extern void SysTick_Init(void);

int main(void)
{
    uint8_t buffer[8];
    // 1. 初始化硬件
    SysTick_Init();
    UART_Init();
    printf("Bootloader V1.0 start...\r\n");

    // 2. 核心魔法：用指针直接指向 Flash 中的 NVS 地址
    ota_info_t *p_ota = (ota_info_t *)NVS_SECTOR_ADDR;

    // 3. 判断状态机的走向
    printf("Checking OTA Status: 0x%08lX\r\n", p_ota->boot_flag);

    if (p_ota->boot_flag == OTA_FLAG_UPDATE) 
    {
        printf("INFO: Update Request Detected! Start copying firmware...\r\n");
        // 这里未来放 Flash 搬运代码 (从 B 区读，写到 A 区)
        // 搬运完成后，把标志位改回 NORMAL，防止无限重启搬运
    }
    else if (p_ota->boot_flag == OTA_FLAG_EMPTY)
    {
        printf("INFO: First run or Flash is empty. Initializing NVS...\r\n");
        // 这里未来放 Flash 写入代码，写入 OTA_FLAG_NORMAL
    }
    else 
    {
        printf("INFO: Normal Boot.\r\n");
    }

    // 4. 最终归宿：跳转到 APP
    printf("Jumping to APP_A (0x%08X)...\r\n", APP_A_ADDR);
    if (boot_check_app(APP_A_ADDR))
    {
        boot_jump_to_app(APP_A_ADDR);
    }
    else
    {
        printf("INFO: No valid app.\r\n");
        printf("\r\nWaiting for YModem packets...\r\n");

        // 判断 YModem_Receive 的返回值 (0表示成功，-1表示失败)
        if (YModem_Receive() == 0) 
        {
            // 接收成功，验证写入的数据
            hal_flash_read(APP_A_ADDR, buffer, 8);
            printf("Flash Address: 0x%08lX\r\n", (uint32_t)APP_A_ADDR);
            printf("Raw Data: ");
            for(int i=0; i<8; i++) printf("%02X ", buffer[i]);
            printf("\r\n");

            printf("OTA Success! Rebooting system...\r\n");
            
            // 极其重要: 触发软复位，让系统重新启动并跳入新固件！
            NVIC_SystemReset(); 
        }
        else
        {
            printf("OTA Failed or Aborted. System halted.\r\n");
            // 如果接收失败，可以在这里死循环，或者干脆也 NVIC_SystemReset() 重新来过
        }
    }

    // 如果程序能跑到这里，说明出了大问题，兜底死循环
    while (1)
    {
        
    }
}

