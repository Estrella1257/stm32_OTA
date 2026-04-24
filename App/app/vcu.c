#include "vcu.h"
#include "throttle.h"
#include <math.h>
#include "key.h"
#include "nvs_api.h"
#include "motor.h"
#include "pid.h"

// 引入 IMU 解算出来的角度
extern float g_vcu_pitch;
extern float g_vcu_roll;

// 全局状态机变量，开机默认处于 INIT 状态
VCU_State_t g_vcu_state = VCU_STATE_INIT;

int16_t global_sim_speed = 0;     
uint8_t global_sim_battery = 100; 
uint8_t global_sim_gear = 0;    
uint8_t global_sim_enable = 0;
double g_ram_odometer_m = 0.0;

extern PI_Controller_t motor_speed_pi;
extern float g_filtered_rpm;

void VCU_Task_50ms(void) 
{
    // 任务 A：极致实时的 UI 视觉层 (完美数学积分法)
    // 公式：转过的圈数 = (当前RPM / 60) * 0.05秒
    // 行驶米数 = 圈数 * 1000米
    float revs_in_50ms = (fabsf(g_filtered_rpm) / 60.0f) * 0.05f;
    float current_delta_m = revs_in_50ms * 1000.0f;
    
    if (current_delta_m > 0.0f) {
        g_ram_odometer_m += current_delta_m; 
    }

    // 任务 B：保护寿命的 NVS 异步落盘层 
    static uint32_t last_saved_odo = 0xFFFFFFFF; 
    if (last_saved_odo == 0xFFFFFFFF) last_saved_odo = g_sys_cfg.total_odometer_m; 

    // 每行驶满 1000 米 (1公里)，才真正触发一次 Flash 写入
    if ((g_ram_odometer_m - last_saved_odo) >= 1000.0) 
    {
        g_sys_cfg.total_odometer_m = (uint32_t)g_ram_odometer_m; 
        last_saved_odo = g_sys_cfg.total_odometer_m;             
        
        // 因为现在它在真实世界里，每骑 1 分多钟才会触发一次。
        // 这 20 毫秒的卡顿在正常的行车过程中是完全可以接受的。
        nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
        
        printf("\r\n[NVS] AUTO-SAVE -> Flash Updated. Odometer: %lu km\r\n", last_saved_odo / 1000);
    }

    // 任务 C：电池模拟逻辑
    static int battery_tick = 0;
    battery_tick++;
    if(battery_tick > 10) { 
        global_sim_battery--;
        if(global_sim_battery == 0) global_sim_battery = 100;
        battery_tick = 0;
    }

    // 任务 D：全局最高优先级：上帝视角的安全防御
    if (g_vcu_state != VCU_STATE_INIT && g_vcu_state != VCU_STATE_FAULT) 
    {
        if (fabsf(g_vcu_roll) > 45.0f) 
        {
            printf("\r\n[VCU] FATAL -> Crash Detected! Roll: %.1f\r\n", g_vcu_roll);
            g_vcu_state = VCU_STATE_FAULT;
            Motor_Disable();  // 瞬间断电
        }
    }

    // 任务 E：核心驱动状态机
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
            // 1. 读取真实油门开度 (0~100)
            uint8_t throttle_percent = Get_Percent();

            // 2. 获取当前档位 (已消抖，绝对纯净)
            DriveMode_t current_mode = Key_Get();
            
            // 3. 动态查表限速机制
            uint8_t speed_limit = 60; // 默认 DRIVE 模式 60km/h
            if (current_mode == MODE_ECO) {
                speed_limit = 25;     // ECO 模式锁死 25km/h
            } else if (current_mode == MODE_SPORT) {
                speed_limit = 85;     // SPORT 模式解除封印
            }

            // 4. 最终速度合成
            global_sim_speed = (throttle_percent * speed_limit) / 100; 

            // 5. 将实际物理档位同步给仪表盘显示
            global_sim_gear = (uint8_t)current_mode + 1; // 变成 1,2,3 传给 ESP32

            if (!global_sim_enable) {
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
        if (g_sys_cfg.pid_kp > 0.1f || g_sys_cfg.pid_kp < 0.0f) {
            printf("\r\n[NVS] WRN -> Corrupted PID detected! Resetting...\r\n");
            g_sys_cfg.pid_kp = 0.01f;
            g_sys_cfg.pid_ki = 0.042f;
            g_sys_cfg.speed_limit_kmh_x10 = 850; 
            nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg); 
        }
        g_ram_odometer_m = (double)g_sys_cfg.total_odometer_m;
        printf("[NVS] OK -> Odo: %.1fm, Kp: %.3f, Ki: %.3f\r\n", 
                g_ram_odometer_m, g_sys_cfg.pid_kp, g_sys_cfg.pid_ki);
    } else if (ret == NVS_ERR_NOT_FOUND) {
        printf("[NVS] WRN -> No valid config found. Formatting Factory Defaults...\r\n");
        
        memset(&g_sys_cfg, 0, sizeof(SystemConfig_t));
        g_sys_cfg.total_odometer_m = 0;       
        g_sys_cfg.pid_kp = 0.01f;  
        g_sys_cfg.pid_ki = 0.042f;            
        g_sys_cfg.speed_limit_kmh_x10 = 850;  
        
        nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
        printf("[NVS] OK -> Factory defaults stored.\r\n");
    }
}