#ifndef PID_H
#define PID_H

typedef struct {
    float Kp;              // 比例系数 (爆发力)
    float Ki;              // 积分系数 (耐力/消除静差)
    
    float integral_error;  // 误差的积分累计值
    float voltage_limit;   // 极限输出电压保护 (防炸管)
} PI_Controller_t;

// 初始化 PID 参数
void PI_Init(PI_Controller_t *pid, float kp, float ki, float limit);

// 执行一次 PI 计算 (必须在定时的中断里调用！)
float PI_Update(PI_Controller_t *pid, float target, float actual, float dt);

#endif