#include "boot.h"
#include <stdio.h>
#include "uart.h"
#include "hal_flash_boot.h"
#include "ymodem.h"
#include "stm32f4xx.h"

extern void SysTick_Init(void);

int main(void)
{
    uint8_t buffer[8];
    // 1. 初始化硬件
    SysTick_Init();
    UART1_Log_Init();
    UART2_YModem_Init();
    printf("\r\n=================================\r\n");
    printf(" Bootloader V1.3 start...\r\n");
    printf("=================================\r\n");

    // 2. 核心魔法：开启 PWR 时钟并解锁 RTC 备份域访问
    // 只有解锁了，才能读到 APP 死前写在 RTC 里的遗言
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    // 3. 读取 RTC 备份寄存器 1 里的标志位
    uint32_t boot_flag = RTC_ReadBackupRegister(RTC_BKP_DR1);
    printf("[SYS] RTC_BKP_DR1: 0x%04lX ", boot_flag);

    // 4. 判断状态机的走向
    if (boot_flag == 0xAAAA) 
    {
        printf("-> [MODE] OTA_UPGRADE\r\n"); 
        printf("[OTA] Triggered by App. Entering firmware update mode...\r\n");
        
        // 【极其重要】: 擦除魔法词，防止刷入固件后无限进入 Bootloader 死循环
        RTC_WriteBackupRegister(RTC_BKP_DR1, 0x0000); 

        // 开始 YModem 接收 (在这里面会发送 'C' 并等待 ESP32 的固件)
        if (YModem_Receive() == 0) 
        {
            // 接收并烧录成功，验证一下写入的数据
            hal_flash_read(APP_START_ADDR, buffer, 8);
            printf("[FLASH] Verify 0x%08lX: ", (uint32_t)APP_START_ADDR);
            for(int i=0; i<8; i++) printf("%02X ", buffer[i]);
            printf("\r\n[OK] Firmware updated. Rebooting system...\r\n");

            // 触发软复位，让系统重新启动并跳入新固件！
            NVIC_SystemReset(); 
        }
        else
        {
            printf("[ERR] OTA Sequence failed. Falling back to reset...\r\n");
            // 接收失败也重启，让系统尝试回退到之前的状态
            NVIC_SystemReset(); 
        }
    }
    else 
    {
        printf("-> [MODE] NORMAL_BOOT\r\n");
        
        // 5. 正常启动：检查 APP 区域有没有合法的代码
        if (boot_check_app(APP_START_ADDR))
        {
            printf("[BOOT] APP found. Jumping to 0x%08X...\r\n\r\n", APP_START_ADDR);
            boot_jump_to_app(APP_START_ADDR);
        }
        else
        {
            printf("[WRN] NO VALID APP at 0x%08X!\r\n", APP_START_ADDR);
            printf("[SYS] Fallback to Emergency OTA mode. Waiting for YModem...\r\n");
            
            // 兜底机制：如果是空板子第一次上电，没有 APP，强制进入 OTA 接收固件
            if (YModem_Receive() == 0) {
                printf("[OK] Recovery success. Rebooting...\r\n");
                NVIC_SystemReset(); 
            }
        }
    }

    // 如果程序能跑到这里，说明出了大问题，兜底死循环
    while (1)
    {
        // Blink LED or something to indicate hard fault
    }
}