#include "debug_shell.h"

SystemConfig_t g_sys_cfg;
NvsContext_t g_nvs_ctx;

void DebugShell_PrintStatus(void) {
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

void DebugShell_ShowMenu(void) {
    printf("\r\n>>> NVS Test Shell Ready <<<\r\n");
    printf("Send '1': Add 100m to Odometer & Save\r\n");
    printf("Send '2': Tune Kp (+0.1) & Save\r\n");
    printf("Send '3': Trigger GC Stress Test (Write 50 times)\r\n");
    printf("Send '4': Toggle ESP32 UI Simulator (ON/OFF)\r\n");
    printf("Send '5': Contrl ESP32 ota cmd trigger\r\n");
    printf("Send 'R': Hard Reboot (Test Data Persistence)\r\n");
}

void DebugShell_ProcessCommand(char cmd) {
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
            DebugShell_PrintStatus();
            break;
        case '4': // 模拟器开关
            global_sim_enable = !global_sim_enable;
            printf("\r\n[VCU] ESP32 Cyberpunk Simulator: %s !!!\r\n", global_sim_enable ? "ONLINE" : "💤 OFFLINE");
            break;
        case '5':
            printf("[SYS] Requesting ESP32 to send OTA trigger...\r\n");
            // 构造一个简单的请求包发给 ESP32
            // 0xAA 0x55 | CMD:0x03 | LEN:0x01 | DATA:0x00 | CHECKSUM:0x03
            uint8_t req_buf[6] = {0xAA, 0x55, 0x03, 0x01, 0x00, 0x03};
            usart2_send_data_dma(req_buf, 6);
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