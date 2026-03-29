#include "imu_task.h"
#include "mpu6050.h"
#include "filter.h"

#define PI 3.14159265358979f
#define GYRO_SCALE 16.4f 
#define IMU_DT 0.01f // 10ms 调度周期

// 实例化两个卡尔曼滤波器
static Kalman_t kf_pitch;
static Kalman_t kf_roll;

// 全局姿态数据 (供 VCU 随时调用)
float g_vcu_pitch = 0.0f;
float g_vcu_roll  = 0.0f;

// IMU 子系统初始化
void IMU_Task_Init(void)
{
    MPU6050_Init();
    
    // 检查设备在线状态
    uint8_t id = MPU6050_GetDeviceID();
    if (id == 0x68) {
        printf("[SYS] MPU6050 Online! ID: 0x%02X\r\n", id);
    } else {
        printf("[SYS] ERR -> MPU6050 Offline! ID: 0x%02X\r\n", id);
    }

    // 初始化两个卡尔曼滤波器
    Kalman_Init(&kf_pitch);
    Kalman_Init(&kf_roll);
}

// IMU 10ms 业务更新 (由定时器驱动，绝对不能有 delay)
void IMU_Task_10ms_Update(void)
{
    short ax, ay, az, gx, gy, gz;
    
    // 1. 读取原始数据 (如果读失败可以做断线保护，这里略过)
    MPU6050_Read_Raw(&ax, &ay, &az, &gx, &gy, &gz);

    // 2. 加速度计计算绝对角度 (作为卡尔曼的观测值)
    float acc_pitch = -atan2((float)ax, (float)az) * (180.0f / PI); 
    float acc_roll  = atan2((float)ay, (float)az) * (180.0f / PI);

    // 3. 陀螺仪计算角速度 (作为卡尔曼的预测依据)
    float gyro_pitch_rate = (float)gy / GYRO_SCALE;
    float gyro_roll_rate  = (float)gx / GYRO_SCALE;

    // 4. 双通道卡尔曼解算！
    g_vcu_pitch = Kalman_GetAngle(&kf_pitch, acc_pitch, gyro_pitch_rate, IMU_DT);
    g_vcu_roll  = Kalman_GetAngle(&kf_roll, acc_roll, gyro_roll_rate, IMU_DT);
}