#include "foc_task.h"
#include "vcu.h"

// 外部引入 VCU 算出来的目标速度 (你的油门和档位结果)
extern int16_t global_sim_speed; 
extern VCU_State_t g_vcu_state;

float g_filtered_rpm = 0.0f;
float target_voltage = 0.0f;

// 这个函数放到 TIM6 中断服务函数 (TIM6_IRQHandler) 里，每 1ms 执行一次
void FOC_Hardware_1ms_ISR(void) 
{
    // 1. 最高优先级：如果 VCU 故障（比如摔车），瞬间切断动力
    if (g_vcu_state == VCU_STATE_FAULT || g_vcu_state == VCU_STATE_STANDBY) {
        target_voltage = 0.0f;
        FOC_ClosedLoop_Voltage_Test(0.0f); // 零电压滑行
        return; // 直接退出，不计算 PI
    }

    // 2. 1000Hz 肌肉层：极速换相
    FOC_ClosedLoop_Voltage_Test(target_voltage);

    // 3. 500Hz 大脑层：降频执行 PI 控制 (2ms 一次)
    static uint8_t div_2ms = 0;
    if (++div_2ms >= 2) 
    {
        div_2ms = 0;
        
        // 采集并滤波 (注意这里的 dt 变成了 0.002s(2ms))
        float raw_rpm = Encoder_Get_Velocity_RPM(0.002f);
        g_filtered_rpm = LowPass_Filter(raw_rpm, g_filtered_rpm, 0.02f, 0.002f); 
        
        // 神经递质交接：将 VCU 的 global_sim_speed 作为目标转速喂给 PI
        // 注意：把 global_sim_speed (km/h 或 比例) 缩放到合适的 RPM 范围
        float target_rpm_request = (float)global_sim_speed * 5.0f; 
        
        // 如果油门为0，彻底断电并锁死积分器
        if (target_rpm_request <= 0.0f) 
        {
            target_voltage = 0.0f;
            motor_speed_pi.integral_error = 0.0f; // 斩断积分爆炸的根源
        } 
        else 
        {
            target_voltage = PI_Update(&motor_speed_pi, target_rpm_request, g_filtered_rpm, 0.002f);
        }
    }
}