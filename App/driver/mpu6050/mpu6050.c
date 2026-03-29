#include "mpu6050.h"
#include "I2C.h"

#define MPU_ADDR 0xD0 // MPU6050 I2C地址 (AD0接地)

// 向MPU6050写入一个字节
u8 MPU_Write_Byte(u8 reg, u8 data)
{
    IIC_Start(); 
    IIC_Send_Byte(MPU_ADDR);    // 发送器件地址+写命令
    if(IIC_Wait_Ack()) { IIC_Stop(); return 1; }
    IIC_Send_Byte(reg);         // 写寄存器地址
    IIC_Wait_Ack();
    IIC_Send_Byte(data);        // 发送数据
    IIC_Wait_Ack();
    IIC_Stop(); 
    return 0;
}

// 从MPU6050读取连续数据
u8 MPU_Read_Len(u8 addr, u8 reg, u8 len, u8 *buf)
{ 
    IIC_Start(); 
    IIC_Send_Byte((addr<<1)|0); // 发送器件地址+写命令
    if(IIC_Wait_Ack()) { IIC_Stop(); return 1; }
    IIC_Send_Byte(reg);         // 写寄存器地址
    IIC_Wait_Ack();
    IIC_Start();
    IIC_Send_Byte((addr<<1)|1); // 发送器件地址+读命令
    IIC_Wait_Ack();
    while(len)
    {
        if(len == 1) *buf = IIC_Read_Byte(0); // 读最后一个字节，发送nACK
        else *buf = IIC_Read_Byte(1);         // 发送ACK
        len--;
        buf++; 
    }    
    IIC_Stop(); 
    return 0;   
}

// 初始化 MPU6050
void MPU6050_Init(void)
{
    IIC_Init();
    MPU_Write_Byte(0x6B, 0x80); // 复位MPU6050
    delay_ms(100);
    MPU_Write_Byte(0x6B, 0x00); // 唤醒MPU6050
    MPU_Write_Byte(0x19, 0x07); // 陀螺仪采样率：1KHz/(1+7) = 125Hz
    MPU_Write_Byte(0x1A, 0x04); // 低通滤波(DLPF)：带宽20Hz (有效抗震动)
    MPU_Write_Byte(0x1B, 0x18); // 陀螺仪量程：±2000 dps
    MPU_Write_Byte(0x1C, 0x00); // 加速度计量程：±2g
}

// 读取加速度和陀螺仪原始数据 (各16位)
void MPU6050_Read_Raw(short *ax, short *ay, short *az, short *gx, short *gy, short *gz)
{
    u8 buf[14];
    // 一次性读取14个字节：加速度(6) + 温度(2) + 陀螺仪(6)
    MPU_Read_Len(0x68, 0x3B, 14, buf); 
    
    *ax = ((u16)buf[0] << 8) | buf[1];
    *ay = ((u16)buf[2] << 8) | buf[3];
    *az = ((u16)buf[4] << 8) | buf[5];
    // 跳过温度 buf[6], buf[7]
    *gx = ((u16)buf[8] << 8) | buf[9];
    *gy = ((u16)buf[10]<< 8) | buf[11];
    *gz = ((u16)buf[12]<< 8) | buf[13];
}

uint8_t MPU6050_GetDeviceID(void)
{
    uint8_t id = 0;
    // 0x68 是 I2C 地址，0x75 是 WHO_AM_I 寄存器
    MPU_Read_Len(0x68, 0x75, 1, &id); 
    return id;
}