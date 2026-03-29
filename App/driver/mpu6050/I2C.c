#include "I2C.h"
#include "delay.h" 

// 初始化 F407 的 IIC 引脚
void IIC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = IIC_SCL_PIN | IIC_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;       // 输出模式
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;      // 开漏输出 (I2C 必须是开漏)
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;   // 速度
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;        // 内部上拉 (如果外部硬件有上拉电阻，可设为 NOPULL)
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 3. 初始化时总线拉高，释放 I2C
    IIC_SCL_H();
    IIC_SDA_H();
}

// 产生IIC起始信号
void IIC_Start(void)
{
    IIC_SDA_H();
    IIC_SCL_H();
    delay_us(4);
    IIC_SDA_L(); // START: SCL为高时，SDA由高变低
    delay_us(4);
    IIC_SCL_L(); // 钳住I2C总线，准备发送或接收数据 
}

// 产生IIC停止信号
void IIC_Stop(void)
{
    IIC_SCL_L();
    IIC_SDA_L(); // STOP: SCL为高时，SDA由低变高
    delay_us(4);
    IIC_SCL_H(); 
    delay_us(4);
    IIC_SDA_H(); // 释放I2C总线
}

// 等待应答信号到来
uint8_t IIC_Wait_Ack(void)
{
    uint8_t ucErrTime = 0;
    IIC_SDA_H(); delay_us(1);      
    IIC_SCL_H(); delay_us(1);     
    while(IIC_SDA_READ()) // 读取 SDA 状态
    {
        ucErrTime++;
        if(ucErrTime > 250)
        {
            IIC_Stop();
            return 1;
        }
    }
    IIC_SCL_L(); // 时钟输出0      
    return 0;  
}

// IIC发送一个字节
void IIC_Send_Byte(uint8_t txd)
{                        
    uint8_t t;   
    IIC_SCL_L(); // 拉低时钟开始数据传输
    for(t = 0; t < 8; t++)
    {              
        if((txd & 0x80) >> 7) IIC_SDA_H();
        else IIC_SDA_L();
        txd <<= 1;       
        delay_us(2);
        IIC_SCL_H();
        delay_us(2); 
        IIC_SCL_L();    
        delay_us(2);
    }    
}       

// IIC读取一个字节
uint8_t IIC_Read_Byte(unsigned char ack)
{
    unsigned char i, receive = 0;
    IIC_SDA_H(); // F4 开漏模式下直接置高即可读取，不需要像F1那样切换输入输出方向
    for(i = 0; i < 8; i++ )
    {
        IIC_SCL_L(); 
        delay_us(2);
        IIC_SCL_H();
        receive <<= 1;
        if(IIC_SDA_READ()) receive++;   
        delay_us(1); 
    }                     
    if (!ack) IIC_NAck();
    else IIC_Ack(); 
    return receive;
}

void IIC_Ack(void)  { IIC_SCL_L(); IIC_SDA_L(); delay_us(2); IIC_SCL_H(); delay_us(2); IIC_SCL_L(); }
void IIC_NAck(void) { IIC_SCL_L(); IIC_SDA_H(); delay_us(2); IIC_SCL_H(); delay_us(2); IIC_SCL_L(); }