#ifndef __I2C_H
#define __I2C_H

#include "stm32f4xx.h" // 必须是 F4 的头文件

// ================= 引脚定义 (STM32F407) =================
// SCL -> PB10, SDA -> PB11
#define IIC_SCL_PORT GPIOB
#define IIC_SCL_PIN  GPIO_Pin_10
#define IIC_SDA_PORT GPIOB
#define IIC_SDA_PIN  GPIO_Pin_11

// ================= IIC 端口高性能操作宏 (适配新版 F4 标准库) =================
// F4 推荐使用 BSRR 寄存器进行原子操作，速度最快且安全
// 往 BSRR 的低 16 位写 1 是置高电平
#define IIC_SCL_H()  (IIC_SCL_PORT->BSRR = IIC_SCL_PIN)
#define IIC_SDA_H()  (IIC_SDA_PORT->BSRR = IIC_SDA_PIN)

// 往 BSRR 的高 16 位写 1 是置低电平 (将原 PIN 值左移 16 位)
#define IIC_SCL_L()  (IIC_SCL_PORT->BSRR = (uint32_t)IIC_SCL_PIN << 16)
#define IIC_SDA_L()  (IIC_SDA_PORT->BSRR = (uint32_t)IIC_SDA_PIN << 16)

// 读取 SDA 引脚电平 (读 IDR 寄存器)
#define IIC_SDA_READ() ((IIC_SDA_PORT->IDR & IIC_SDA_PIN) ? 1 : 0)

// ================= 函数声明 =================
void IIC_Init(void);
void IIC_Start(void);
void IIC_Stop(void);
void IIC_Send_Byte(uint8_t txd);
uint8_t IIC_Read_Byte(unsigned char ack);
uint8_t IIC_Wait_Ack(void);
void IIC_Ack(void);
void IIC_NAck(void);

#endif