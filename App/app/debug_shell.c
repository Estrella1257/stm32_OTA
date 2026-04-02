#include "debug_shell.h"
#include "vcu.h"

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
            
    printf("------------- VCU PAYLOAD --------------\r\n");
    printf("[VCU] Odometer      : %lu m\r\n", g_sys_cfg.total_odometer_m);
    printf("[VCU] PID Kp        : %.2f\r\n", g_sys_cfg.pid_kp);
    printf("[VCU] Speed Limit   : %.1f km/h\r\n", g_sys_cfg.speed_limit_kmh_x10 / 10.0f);
    printf("============================================\r\n\r\n");
}

void DebugShell_ShowMenu(void) {
    printf("\r\n>>> NVS Test Shell Ready <<<\r\n");
    printf("Send '1': Add 100m to Odometer & Save\r\n");
    printf("Send '2': Tune Kp (+0.1) & Save\r\n");
    printf("Send '3': Trigger GC Stress Test (Write 50 times)\r\n");
    printf("Send '4': Toggle ESP32 UI Simulator (ON/OFF)\r\n");
    printf("Send 'F': Manual FAULT triggered\r\n");
    printf("Send 'C': Clear Fault. Return to STANDBY\r\n");
    printf("Send 'R': Hard Reboot (Test Data Persistence)\r\n");
    printf("Send 'B': Stop feeding the dog (Test Watch Dog)\r\n");
}

void DebugShell_ProcessCommand(char cmd) {
    switch (cmd) {
        case '1':
            g_sys_cfg.total_odometer_m += 100;
            printf("[CMD] EVT -> Odometer +100m (New: %lu m). Saving...\r\n", g_sys_cfg.total_odometer_m);
            nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
            break;
        case '2':
            g_sys_cfg.pid_kp += 0.1f;
            printf("[CMD] EVT -> Kp tuned to %.2f. Saving...\r\n", g_sys_cfg.pid_kp);
            nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
            break;
        case '3': 
            printf("\r\n[NVS] WRN -> STARTING STRESS TEST (50 Writes)...\r\n");
            for (int i = 0; i < 50; i++) {
                g_sys_cfg.total_odometer_m += 1; 
                if (nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg) == NVS_OK) {
                    printf("."); // 保持紧凑
                } else {
                    printf("\r\n[NVS] ERR -> GC FAILED at iter %d!\r\n", i);
                    break;
                }
            }
            printf("\r\n[NVS] OK  -> STRESS TEST COMPLETE.\r\n");
            DebugShell_PrintStatus();
            break;
        case '4': 
            global_sim_enable = !global_sim_enable;
            printf("[VCU] MODE -> Simulator %s\r\n", global_sim_enable ? "ENABLED" : "DISABLED");
            break;
        case 'F':
            printf("\r\n[SYS] WRN -> Manual FAULT triggered!\r\n");
            g_vcu_state = VCU_STATE_FAULT;
            break;
        case 'C':
            printf("\r\n[SYS] OK -> Fault Cleared. Returning to STANDBY.\r\n");
            g_vcu_state = VCU_STATE_STANDBY;
            break;
        case 'R':
        case 'r':
            printf("[SYS] CMD -> Manual System Reset requested.\r\n");
            NVIC_SystemReset(); 
            break;
        case 'B':
            printf("\r\n[SYS] -> Stop feeding the dog...\r\n");
            // 制造一个死循环，彻底卡死前台任务，看门狗将在 1 秒后咬死系统
            while(1) {
            }
            break;
        default:
            printf("[CMD] ERR -> Unknown instruction: '%c'\r\n", cmd);
            break;
    }
}