#include "vcu.h"

int16_t global_sim_speed = 0;     
uint8_t global_sim_battery = 100; 
uint8_t global_sim_gear = 0;    
uint8_t global_sim_enable = 0;

void VCU_Task_50ms(void) {
    if (global_sim_enable) {
        // 速度逻辑
        global_sim_speed++; 
        if(global_sim_speed > 85) global_sim_speed = 0;

        // 电量逻辑
        static int battery_tick = 0;
        battery_tick++;
        if(battery_tick > 10) { 
            global_sim_battery--;
            if(global_sim_battery == 0) global_sim_battery = 100;
            battery_tick = 0;
        }

        // 档位逻辑
        if (global_sim_speed == 0) global_sim_gear = 0; 
        else if (global_sim_speed < 30) global_sim_gear = 1;
        else if (global_sim_speed < 60) global_sim_gear = 2;
        else global_sim_gear = 3; 

        // 封包并发送
        Protocol_Send_Telemetry(global_sim_speed, global_sim_battery, global_sim_gear);
    }
}

void VCU_Config_Init(void)
{
    printf("\r\n[SYS] BOOT -> App Booting at 0x08010000...\r\n");
    printf("[NVS] INIT -> Initializing NVS subsystem...\r\n");
    
    int ret = nvs_init(&g_nvs_ctx, &stm32_nvs_port);
    if (ret != NVS_OK) {
        printf("[NVS] FATAL -> Init failed (Code: %d). System Halted.\r\n", ret); 
        while(1); // 初始化失败，死机保护
    }

    ret = nvs_load(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
    if (ret == NVS_OK) {
        printf("[NVS] OK -> Loaded existing configuration!\r\n");
    } else if (ret == NVS_ERR_NOT_FOUND) {
        printf("[NVS] WRN -> No valid config found. Formatting Factory Defaults...\r\n");
        
        memset(&g_sys_cfg, 0, sizeof(SystemConfig_t));
        g_sys_cfg.total_odometer_m = 0;       
        g_sys_cfg.pid_kp = 1.5f;              
        g_sys_cfg.speed_limit_kmh_x10 = 250;  
        
        nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
        printf("[NVS] OK -> Factory defaults stored.\r\n");
    }
}