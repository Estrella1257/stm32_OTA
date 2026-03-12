#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "usart.h"
#include "nvs_api.h"        
#include "nvs_types.h"      

// 全局变量区
volatile uint8_t g_rx_data = 0;
volatile uint8_t g_rx_flag = 0;

NvsContext_t   g_nvs_ctx;
SystemConfig_t g_sys_cfg;

extern const NvsPort_t stm32_nvs_port;

// 串口中断回调 
static void usart_received(uint8_t data)
{
    if (data == '\r' || data == '\n' || data == ' ') return;
    g_rx_data = data;
    g_rx_flag = 1;
}

// LED 驱动 
void led(void) {
    led_init(&led1); 
    led_init(&led2); 
    led_init(&led3);
    led_on(&led1);   
    led_on(&led2);   
    led_on(&led3);
}

// 辅助函数：打印当前 NVS 和系统状态
static void Print_System_Status(void) {
    printf("\r\n================ NVS STATUS ================\r\n");
    printf("[NVS] Active Sector : 0x%08lX\r\n", g_nvs_ctx.active_sector_addr);
    printf("[NVS] Standby Sector: 0x%08lX\r\n", g_nvs_ctx.standby_sector_addr);
    printf("[NVS] Generation    : %lu\r\n", g_nvs_ctx.active_generation);
    printf("[NVS] Next Seq ID   : %lu\r\n", g_nvs_ctx.next_seq);
    
    uint32_t used_bytes = g_nvs_ctx.next_write_addr - (g_nvs_ctx.active_sector_addr + sizeof(NvsSectorHeader_t));
    uint32_t total_bytes = NVS_SECTOR_SIZE - sizeof(NvsSectorHeader_t);
    printf("[NVS] Sector Usage  : %lu / %lu Bytes (%.1f%%)\r\n", 
            used_bytes, total_bytes, (float)used_bytes / total_bytes * 100.0f);
            
    printf("------------- APP PAYLOAD --------------\r\n");
    printf("[APP] Odometer      : %lu m\r\n", g_sys_cfg.total_odometer_m);
    printf("[APP] PID Kp        : %.2f\r\n", g_sys_cfg.pid_kp);
    printf("[APP] Speed Limit   : %.1f km/h\r\n", g_sys_cfg.speed_limit_kmh_x10 / 10.0f);
    printf("============================================\r\n\r\n");
}

int main(void)
{
    __disable_irq(); 

    //SysTick (系统滴答定时器) 状态机复位
    SysTick->CTRL = 0;
    //向控制及状态寄存器 (SysTick Control and Status Register) 写入 0
    SysTick->LOAD = 0;
    //清零重装载值寄存器 (SysTick Reload Value Register)
    SysTick->VAL  = 0;
    //清零当前值寄存器 (SysTick Current Value Register)

    //NVIC (嵌套向量中断控制器) 寄存器阵列洗白
    for (int i = 0; i < 8; i++) {
        // ICER (Interrupt Clear-Enable Register): 中断清除使能寄存器
        NVIC->ICER[i] = 0xFFFFFFFF;
        // ICPR (Interrupt Clear-Pending Register): 中断清除挂起寄存器
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    SCB->VTOR = 0x08010000;

    board_lowlevel_init();
    usart_init();
    usart_receive_register(usart_received);
    led();

    // 强杀 ORE 只需要保留在 usart_init 刚配完后的这一次清洗，以及 usart.c 的中断里即可
    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET) {
        USART_ReceiveData(USART1); 
    }
    USART_ClearFlag(USART1, USART_FLAG_RXNE);

    __enable_irq();

    printf("\r\n\r\n[SYS] App Booting at 0x08010000...\r\n");
    printf("[NVS] Initializing NVS subsystem...\r\n");
    
    int ret = nvs_init(&g_nvs_ctx, &stm32_nvs_port);
    if (ret != NVS_OK) {
        printf("[NVS] ERROR: Init failed with code %d!\r\n", ret);
        while(1); 
    }

    ret = nvs_load(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
    if (ret == NVS_OK) {
        printf("[NVS] SUCCESS: Loaded existing configuration!\r\n");
    } else if (ret == NVS_ERR_NOT_FOUND) {
        printf("[NVS] WARNING: No valid config found. Formatting Factory Defaults...\r\n");
        memset(&g_sys_cfg, 0, sizeof(SystemConfig_t));
        g_sys_cfg.total_odometer_m = 0;       
        g_sys_cfg.pid_kp = 1.5f;              
        g_sys_cfg.speed_limit_kmh_x10 = 250;  
        
        nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
    }

    Print_System_Status();
    
    printf(">>> NVS Test Shell Ready <<<\r\n");
    printf("Send '1': Add 100m to Odometer & Save\r\n");
    printf("Send '2': Tune Kp (+0.1) & Save\r\n");
    printf("Send '3': Trigger GC Stress Test (Write 50 times)\r\n");
    printf("Send 'R': Hard Reboot (Test Data Persistence)\r\n");

    uint32_t heartbeat = 0;
    while (1) 
    {
        if (g_rx_flag == 1)
        {
            char cmd = g_rx_data;
            g_rx_flag = 0; 
            led_toggle(&led1);

            printf("\r\n[DEBUG] Received: '%c' (Hex: 0x%02X)\r\n", 
                   (cmd >= 32 && cmd <= 126) ? cmd : '.', cmd);

            switch (cmd) {
                case '1':
                    g_sys_cfg.total_odometer_m += 100;
                    printf("[*] Riding 100m... Total: %lu m. Saving...\r\n", g_sys_cfg.total_odometer_m);
                    nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
                    break;
                case '2':
                    g_sys_cfg.pid_kp += 0.1f;
                    printf("[*] Tuning Kp -> %.2f. Saving...\r\n", g_sys_cfg.pid_kp);
                    nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
                    break;
                case '3': // 终极 GC 压力测试
                    printf("\r\n[!] STARTING NVS STRESS TEST...\r\n");
                    printf("[!] Pushing 50 records rapidly to trigger Garbage Collection.\r\n");
                    for (int i = 0; i < 50; i++) {
                        g_sys_cfg.total_odometer_m += 1; // 每次加 1 米，制造大量不同数据
                        int save_ret = nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
                        
                        if (save_ret == NVS_OK) {
                            // 打印小点，表示写入成功
                            printf("."); 
                        } else {
                            printf("\r\n[!] SAVE FAILED at iteration %d! Code: %d\r\n", i, save_ret);
                            break;
                        }
                    }
                    printf("\r\n[!] STRESS TEST DONE.\r\n");
                    Print_System_Status(); // 测试完立马打印一次状态，看有没有切扇区
                    break;
                case 'R':
                case 'r':
                    printf("[!] Resetting...\r\n");
                    NVIC_SystemReset(); 
                    break;
                default:
                    printf("[-] Unknown Command.\r\n");
                    break;
            }
        }
        
        heartbeat++;
        if (heartbeat % 500000 == 0) led_toggle(&led2); 
    }
}