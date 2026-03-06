#include "stm32f4xx.h"
#include "uart.h"
#include "ymodem.h"

// APB2 总线频率是 84MHz (STM32F407 标准主频 168MHz 下)
#define PCLK2_FREQ      84000000
#define BAUDRATE        115200

//#define YMODEM_RX_BUFFER_SIZE  1032
uint8_t ymodem_rx_buffer[YMODEM_RX_BUFFER_SIZE];
volatile uint16_t ymodem_rx_len = 0;
volatile uint8_t  ymodem_rx_flag = 0;

void UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    DMA_InitTypeDef DMA_InitStructure;

    // 1. 开启时钟：GPIOA、USART1、DMA2
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

    // 2. 引脚复用映射
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);  // PA9 -> TX
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1); // PA10 -> RX

    // 3. 配置 GPIO
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;      // 复用功能
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;    // 推挽
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;      // 上拉
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. 配置 USART1
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);

    // 开启串口空闲中断 (IDLE) 和 DMA 接收请求
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);

    // 5. 配置 DMA2_Stream2_Channel4 (负责接收)
    DMA_DeInit(DMA2_Stream2);
    DMA_InitStructure.DMA_Channel = DMA_Channel_4;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)ymodem_rx_buffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_InitStructure.DMA_BufferSize = YMODEM_RX_BUFFER_SIZE;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA2_Stream2, &DMA_InitStructure);

    // 使能 DMA
    DMA_Cmd(DMA2_Stream2, ENABLE);

    // 6. 配置 NVIC 中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 7. 最后使能串口
    USART_Cmd(USART1, ENABLE);
}

// 发送一个字符
void UART_SendChar(uint8_t ch)
{
    // 等待发送数据寄存器为空
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, ch);
}

// 供 printf 调用的底层重定向函数
int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++) {
        UART_SendChar((uint8_t)ptr[i]);
    }
    return len;
}

void USART1_IRQHandler(void)
{
    // 判断是否是串口空闲中断 (IDLE)
    if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET)
    {
        // 1. 清除 IDLE 标志位 (标准库的读 SR 和 DR 操作)
        volatile uint32_t temp = USART1->SR;
        temp = USART1->DR;
        (void)temp;    // 不让它报“变量未使用”的警告

        // 2. 暂停 DMA
        DMA_Cmd(DMA2_Stream2, DISABLE);
        while (DMA_GetCmdStatus(DMA2_Stream2) != DISABLE); // 确保已关闭

        // 3. 计算长度：总长度 - DMA剩余待传数量
        ymodem_rx_len = YMODEM_RX_BUFFER_SIZE - DMA_GetCurrDataCounter(DMA2_Stream2);
        
        // 4. 修改标志位
        ymodem_rx_flag = 1;

        // 5. 必须在这里清除 DMA 的各种标志位
        // 否则下一个 1K 包越过半数线留下的 HTIF 标志，会导致 DMA 无法再次开启
        DMA_ClearFlag(DMA2_Stream2, DMA_FLAG_TCIF2 | DMA_FLAG_HTIF2 | DMA_FLAG_TEIF2);

        // 6. 重新配置 DMA 准备下一次接收
        DMA_SetCurrDataCounter(DMA2_Stream2, YMODEM_RX_BUFFER_SIZE);
        DMA_Cmd(DMA2_Stream2, ENABLE);
    }
}