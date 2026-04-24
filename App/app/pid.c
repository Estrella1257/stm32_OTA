#include "pid.h"

void PI_Init(PI_Controller_t *pid, float kp, float ki, float limit)
{
    pid->Kp = kp;
    pid->Ki = ki;
    pid->integral_error = 0.0f;
    pid->voltage_limit = limit;
}

// 核心控制大脑：输入你想要的速度和真实的速度，输出应该给电机多大的电压 (Uq)
float PI_Update(PI_Controller_t *pid, float target, float actual, float dt)
{
    // 1. 计算当前误差
    float error = target - actual;

    // 2. 计算比例项 (P)：误差越大，这一巴掌扇得越狠
    float proportional = pid->Kp * error;

    // 3. 计算积分项 (I)：只要误差一直存在，就随着时间不断累加力量
    pid->integral_error += pid->Ki * error * dt;

    // 【极其重要的抗积分饱和 (Anti-windup)】：防止电机卡住时，积分项无限爆炸
    if (pid->integral_error > pid->voltage_limit) pid->integral_error = pid->voltage_limit;
    if (pid->integral_error < -pid->voltage_limit) pid->integral_error = -pid->voltage_limit;

    // 4. 合并输出：P 和 I 共同发力
    float output_voltage = proportional + pid->integral_error;

    // 5. 最终输出限幅：坚决不能超过我们设定的安全电压 (比如 5V)
    if (output_voltage > pid->voltage_limit) output_voltage = pid->voltage_limit;
    if (output_voltage < -pid->voltage_limit) output_voltage = -pid->voltage_limit;

    return output_voltage;
}