#include "vofa.h"
#include "usart.h" 

// JustFloat 协议帧尾
const uint8_t VOFA_Tail[4] = {0x00, 0x00, 0x80, 0x7F};

// 发送一帧 JustFloat 数据到 VOFA+
// data_array: 浮点数数组指针
// channel_num: 通道数量 (我们要发Ax, Ay, Az就是3)
void VOFA_JustFloat_Send(float *data_array, uint8_t channel_num)
{
    uint8_t *byte_ptr = (uint8_t *)data_array;
    uint8_t i;
    
    // 1. 发送 Float 数据
    for(i = 0; i < (channel_num * 4); i++)
    {
        USART_ClearFlag(USART3, USART_FLAG_TC);
        USART_SendData(USART3, byte_ptr[i]);
        while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET); 
    }
    
    // 2. 发送帧尾
    for(i = 0; i < 4; i++)
    {
        USART_ClearFlag(USART3, USART_FLAG_TC);
        USART_SendData(USART3, VOFA_Tail[i]);
        while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
    }
}