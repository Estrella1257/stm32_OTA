#include "filter.h"

// 初始化卡尔曼滤波器的参数 (这几个初始值是无数工程师调出来的黄金经验值)
void Kalman_Init(Kalman_t *kf) 
{
    kf->Q_angle   = 0.001f;   // 陀螺仪积分的方差 (很小，因为短时间内陀螺仪很准)
    kf->Q_bias    = 0.003f;   // 陀螺仪零偏的方差
    kf->R_measure = 0.03f;    // 加速度计的方差 (比Q大很多，因为加速度计太敏感，充满毛刺)
    
    kf->angle     = 0.0f;     // 初始角度
    kf->bias      = 0.0f;     // 初始零偏
    
    kf->P[0][0] = 0.0f; kf->P[0][1] = 0.0f;
    kf->P[1][0] = 0.0f; kf->P[1][1] = 0.0f;
}

// ==========================================================
// 获取卡尔曼最优角度
// kf: 滤波器结构体指针
// newAngle: 加速度计刚刚算出来的粗糙角度 (带有巨大毛刺)
// newRate:  陀螺仪刚刚读出来的角速度 (带有零偏)
// dt:       采样周期 (我们用的是 0.01s，即 10ms)
// ==========================================================
float Kalman_GetAngle(Kalman_t *kf, float newAngle, float newRate, float dt) 
{
    // ------------------- 第一阶段：预测 (Predict) -------------------
    // 1. 先验状态预测：用陀螺仪积分预测当前角度 (注意减去了估计出来的零偏 bias)
    float rate = newRate - kf->bias;
    kf->angle += dt * rate;

    // 2. 先验误差协方差预测：算算这次预测的误差有多大
    kf->P[0][0] += dt * (dt * kf->P[1][1] - kf->P[0][1] - kf->P[1][0] + kf->Q_angle);
    kf->P[0][1] -= dt * kf->P[1][1];
    kf->P[1][0] -= dt * kf->P[1][1];
    kf->P[1][1] += kf->Q_bias * dt;

    // ------------------- 第二阶段：更新 (Update) -------------------
    // 3. 计算卡尔曼增益 (Kalman Gain)：决定信模型多少，信测量多少
    float S = kf->P[0][0] + kf->R_measure; // 估算误差 + 测量误差
    float K[2]; // 两个增益，一个给角度，一个给零偏
    K[0] = kf->P[0][0] / S;
    K[1] = kf->P[1][0] / S;

    // 4. 后验状态更新：结合加速度计的真实测量值 (newAngle) 算出最优解
    float y = newAngle - kf->angle; // 测量值与预测值的残差 (Innovation)
    kf->angle += K[0] * y;          // 修正角度
    kf->bias  += K[1] * y;          // 顺便把陀螺仪的零偏也修正了！

    // 5. 后验误差协方差更新：为下一次 10ms 的循环准备
    float P00_temp = kf->P[0][0];
    float P01_temp = kf->P[0][1];

    kf->P[0][0] -= K[0] * P00_temp;
    kf->P[0][1] -= K[0] * P01_temp;
    kf->P[1][0] -= K[1] * P00_temp;
    kf->P[1][1] -= K[1] * P01_temp;

    // 凯旋而归：返回最优估计角度！
    return kf->angle;
}