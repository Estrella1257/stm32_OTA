#ifndef __KALMAN_FILTER_H
#define __KALMAN_FILTER_H

#include "stm32f4xx.h"

// 卡尔曼滤波器结构体
typedef struct {
    float Q_angle;    // 过程噪声方差：对陀螺仪积分角度的信任度 (越小越相信陀螺仪)
    float Q_bias;     // 过程噪声方差：对陀螺仪零偏漂移的信任度
    float R_measure;  // 测量噪声方差：对加速度计的信任度 (越大表示加速度计毛刺越多，越不信它)
    
    float angle;      // 内部状态：最优估计角度 (最终结果)
    float bias;       // 内部状态：最优估计的陀螺仪零偏
    
    float P[2][2];    // 误差协方差矩阵 2x2
} Kalman_t;

// 函数声明
void Kalman_Init(Kalman_t *kf);
float Kalman_GetAngle(Kalman_t *kf, float newAngle, float newRate, float dt);

#endif