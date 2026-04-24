#include "foc.h"
#include "motor.h"
#include "encoder.h"
#include <math.h>

#define PI         3.1415926535f
#define _2PI_3     2.0943951023f  // 120度 (2*PI/3)
#define V_SUPPLY   12.0f          // 你的电源电压：12V

// 辅助宏函数：求最大最小值
#define FMAX(a, b) ((a) > (b) ? (a) : (b))
#define FMIN(a, b) ((a) < (b) ? (a) : (b))

// SVPWM 空间矢量调制 (根据电角度和电压生成 3 相占空比)
void FOC_Set_Phase_Voltage(float Uq, float angle_el) 
{
    // 1. 纯正弦波生成 (SPWM)
    // 假设 D 轴电压为 0，只给 Q 轴 (产生扭矩的轴) 施加电压 Uq
    float Ua = Uq * sinf(angle_el);
    float Ub = Uq * sinf(angle_el - _2PI_3);
    float Uc = Uq * sinf(angle_el + _2PI_3);

    // 2. 空间矢量调制注入 (SVPWM 核心魔法：中心对齐，生成马鞍波)
    // 找出当前三相电压里的最大值和最小值
    float Umax = FMAX(Ua, FMAX(Ub, Uc));
    float Umin = FMIN(Ua, FMIN(Ub, Uc));
    
    // 计算零序分量 (Neutral Point Shift)
    float center = (V_SUPPLY / 2.0f) - ((Umax + Umin) / 2.0f);

    // 将三相电压统一下移或上移，使其完美居中于供电电压的中心
    Ua += center;
    Ub += center;
    Uc += center;

    // 3. 映射到 PWM 占空比 (0.0 ~ 1.0)
    // 限制在 0~1 之间，防止溢出炸管
    float duty_a = FMAX(0.0f, FMIN(1.0f, Ua / V_SUPPLY));
    float duty_b = FMAX(0.0f, FMIN(1.0f, Ub / V_SUPPLY));
    float duty_c = FMAX(0.0f, FMIN(1.0f, Uc / V_SUPPLY));

    // 4. 下发给底层的 TIM8 (满量程 4200)
    Motor_Set_PWM((uint16_t)(duty_a * 4200), 
                  (uint16_t)(duty_b * 4200), 
                  (uint16_t)(duty_c * 4200));
}

// 全局变量：存储零点偏移量
float g_zero_electric_offset = 0.0f;

// 上电寻零校准 (Sensor Alignment)
void FOC_Align_Sensor(void)
{
    // 1. 强行输出一个电角度为 0 的静止磁场，电压给 2.5V
    // 这会在电机内部产生一个强磁力，把转子死死吸在电角度 0 度的位置
    FOC_Set_Phase_Voltage(1.5f, 0.0f); 
    
    // 2. 阻塞等待 1 到 2 秒，让转子完全停止晃动
    //delay_ms(1500); 
    for (int i = 0; i < 150; i++) 
    {
        delay_ms(10);  // 每次只等 10ms
        IWDG_Feed();   // 疯狂喂狗，绝对饿不死
    }
    
    // 3. 读取此时编码器的原始脉冲数 (0 ~ 4095)
    int32_t raw_count = Encoder_Get_RawCount();
    
    // 4. 将脉冲数转换为物理机械角度 (弧度制：0 ~ 2*PI)
    // 注意：用取余防止由于之前手转电机导致脉冲数超过一圈
    float mech_angle = ((float)(raw_count % 4096) / 4096.0f) * 2.0f * PI;
    if(mech_angle < 0.0f) mech_angle += (2.0f * PI); // 保证是正数

    // 5. 算出此时对应的电角度，这就是我们永远的偏移量
    g_zero_electric_offset = mech_angle * POLE_PAIRS;
    
    // 6. 松开电机
    FOC_Set_Phase_Voltage(0.0f, 0.0f);
}

// 闭环电压模式 (真正的 FOC 换相)
void FOC_ClosedLoop_Voltage_Test(float target_voltage)
{
    // 1. 实时读取编码器脉冲
    int32_t raw_count = Encoder_Get_RawCount();
    
    // 2. 转换为机械角度
    float mech_angle = ((float)(raw_count % 4096) / 4096.0f) * 2.0f * PI;
    if(mech_angle < 0.0f) mech_angle += (2.0f * PI);

    // 3. 乘上极对数，减去开机校准的偏移量，得到【真实的绝对电角度】
    float current_elec_angle = (mech_angle * POLE_PAIRS) - g_zero_electric_offset;
    
    // 规范化到 0 ~ 2*PI 之间
    while (current_elec_angle < 0.0f) current_elec_angle += (2.0f * PI);
    while (current_elec_angle > (2.0f * PI)) current_elec_angle -= (2.0f * PI);

    // 4. 【FOC 最伟大的魔法】：正交牵引！
    // 既然我们知道了转子现在的电角度，为了产生最大的旋转扭矩，
    // 我们必须在转子前方刚好 90度 (即 PI/2 弧度) 的地方生成磁场，永远拉着它跑！
    // 如果 target_voltage 是负数，它就会反向牵引，实现反转
    float commutation_angle = current_elec_angle + (PI / 2.0f);
    
    // 规范化
    if (commutation_angle > (2.0f * PI)) commutation_angle -= (2.0f * PI);

    // 5. 将这根完美的“90度胡萝卜”喂给 SPWM 算法
    FOC_Set_Phase_Voltage(target_voltage, commutation_angle);
}

// 极其优雅的一阶低通滤波器
// new_val: 最新算出来的原始速度
// prev_val: 上一次的滤波结果
// Tf: 滤波时间常数 (越大越平滑，但也越迟钝。推荐 0.01 到 0.05)
// dt: 运行周期 (0.01s)
float LowPass_Filter(float new_val, float prev_val, float Tf, float dt)
{
    float alpha = dt / (Tf + dt);
    return (alpha * new_val) + ((1.0f - alpha) * prev_val);
}