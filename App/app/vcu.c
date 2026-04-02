#include "vcu.h"
#include <math.h>

// 引入 IMU 解算出来的角度
extern float g_vcu_pitch;
extern float g_vcu_roll;

// 全局状态机变量，开机默认处于 INIT 状态
VCU_State_t g_vcu_state = VCU_STATE_INIT;

int16_t global_sim_speed = 0;     
uint8_t global_sim_battery = 100; 
uint8_t global_sim_gear = 0;    
uint8_t global_sim_enable = 0;

void VCU_Task_50ms(void) 
{
    // 0. 全局最高优先级：上帝视角的安全防御
    // 无论在什么状态下，只要车翻了，瞬间切断动力
    if (g_vcu_state != VCU_STATE_INIT && g_vcu_state != VCU_STATE_FAULT) 
    {
        // 如果左右倾斜超过 45 度，判定为摔车
        if (fabs(g_vcu_roll) > 45.0f) 
        {
            printf("\r\n[VCU] FATAL -> Crash Detected! Roll: %.1f\r\n", g_vcu_roll);
            g_vcu_state = VCU_STATE_FAULT;
            
            // TODO: 未来在这里调用 SimpleFOC_Disable(); 瞬间断电
        }
    }

    // 1. 核心状态机 (Switch-Case 架构)
    switch (g_vcu_state) 
    {
        case VCU_STATE_INIT:
            // 等待外设初始化稳定（比如等 NVS 读完，等电机对极完成）
            // 如果一切就绪，自动进入待机状态
            printf("[VCU] INFO -> Init Complete. Entering STANDBY mode.\r\n");
            g_vcu_state = VCU_STATE_STANDBY;
            break;

        case VCU_STATE_STANDBY:
            // 驻车状态：电机安全锁死，清空所有的目标速度
            global_sim_speed = 0; 
            
            // 这里可以检测“捏刹车+按启动键”才能进入驾驶模式
            // 为了你现在测试方便，只要开启了模拟器，就自动进入驾驶模式
            if (global_sim_enable) {
                printf("[VCU] INFO -> Engine Started! Entering DRIVING mode.\r\n");
                g_vcu_state = VCU_STATE_DRIVING;
            }
            break;

        case VCU_STATE_DRIVING:
            // 正常行驶状态：这里执行你之前的模拟器速度变化逻辑
            if (global_sim_enable) {
                global_sim_speed++; 
                if(global_sim_speed > 85) global_sim_speed = 0;

                // 电量消耗模拟
                static int battery_tick = 0;
                battery_tick++;
                if(battery_tick > 10) { 
                    global_sim_battery--;
                    if(global_sim_battery == 0) global_sim_battery = 100;
                    battery_tick = 0;
                }

                // 档位模拟
                if (global_sim_speed == 0) global_sim_gear = 0; 
                else if (global_sim_speed < 30) global_sim_gear = 1;
                else if (global_sim_speed < 60) global_sim_gear = 2;
                else global_sim_gear = 3; 
            } else {
                // 如果关闭了模拟器，退回驻车状态
                printf("[VCU] INFO -> Engine Stopped. Returning to STANDBY.\r\n");
                g_vcu_state = VCU_STATE_STANDBY;
            }
            break;

        case VCU_STATE_FAULT:
            // 故障死锁状态：系统瘫痪，什么都不做，只能等重启或者专门的清除故障指令
            global_sim_speed = 0; // 速度归零
            global_sim_gear = 0;  // 档位挂空
            break;

        default:
            g_vcu_state = VCU_STATE_FAULT; // 防跑飞
            break;
    }

    // 2. 遥测数据上报 (打包发送给 ESP32)
    // 建议：你可以把状态机的状态 g_vcu_state 也打包进这帧数据里，让 ESP32 知道车是坏了还是待机
    Protocol_Send_Telemetry(global_sim_speed, global_sim_battery, global_sim_gear);
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