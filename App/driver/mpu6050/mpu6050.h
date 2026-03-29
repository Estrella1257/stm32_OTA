#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f4xx.h"

// ================= 函数声明 =================
void MPU6050_Init(void);
// 读取原始数据：三轴加速度(ax,ay,az) 和 三轴角速度(gx,gy,gz)
void MPU6050_Read_Raw(short *ax, short *ay, short *az, short *gx, short *gy, short *gz);

// 底层I2C封装 (可根据需要外部调用)
uint8_t MPU_Write_Byte(uint8_t reg, uint8_t data);
uint8_t MPU_Read_Len(uint8_t addr, uint8_t reg, uint8_t len, uint8_t *buf);
uint8_t MPU6050_GetDeviceID(void);

#endif