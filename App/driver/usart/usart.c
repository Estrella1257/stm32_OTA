#include "usart.h"
#include <string.h>
#include <stdio.h>

static usart_receive_callback_t receive_callback = NULL;

void usart1_init(void) 
{
    USART_DeInit(USART1);

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    NVIC_InitTypeDef NVIC_InitStruct;
    memset(&NVIC_InitStruct, 0, sizeof(NVIC_InitTypeDef));
    NVIC_InitStruct.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    USART_InitTypeDef USART_InitStruct;
    USART_StructInit(&USART_InitStruct);
    USART_InitStruct.USART_BaudRate = 115200;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &USART_InitStruct);

    USART_Cmd(USART1, ENABLE);
    USART_ReceiveData(USART1);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
}

void usart1_send_data(const char str[])
{
    int length = strlen(str);
    for (uint16_t i = 0; i < length; i++) {
        USART_ClearFlag(USART1, USART_FLAG_TC);
        USART_SendData(USART1, str[i]);
        while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);

    }
}

int fputc(int c,FILE *stream)
{
    (void)stream;

    USART_ClearFlag(USART1, USART_FLAG_TC);
    USART_SendData(USART1,(uint8_t)c);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);

    return c;
}


void usart1_receive_register(usart_receive_callback_t callback)
{
    receive_callback = callback;
}

void USART1_IRQHandler(void)
{
    // 护盾：万一因为别的中断卡顿导致溢出，强行疏通！
    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET) {
        USART_ReceiveData(USART1); 
    }

    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t data = (uint8_t)USART_ReceiveData(USART1);
        
        // 防火墙：直接扔掉回车和换行，只留纯指令
        if (data == '\r' || data == '\n' || data == ' ') {
            return; 
        }

        if (receive_callback) {
            receive_callback(data);
        }
    } 
}

// 缓冲区大小，根据你的 9 字节协议，设为 128 或 256 足够了，必须是 2 的幂 
#define USART2_RX_BUF_SIZE 256
#define USART2_TX_BUF_SIZE 128

// DMA 接收映射内存，千万不要加 static，如果用 MDK 记得不要让它被优化掉 
uint8_t usart2_rx_dma_buf[USART2_RX_BUF_SIZE];
uint8_t usart2_tx_dma_buf[USART2_TX_BUF_SIZE];

// 软件读指针，写指针由硬件 DMA 的 NDTR 寄存器掌管 
static uint16_t rx_read_pos = 0; 
static usart_receive_callback_t usart2_frame_callback = NULL;

void usart2_init_dma(void) 
{
    USART_DeInit(USART2);

    /* --- GPIO 初始化 (与你原来一致) --- */
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

    /* --- USART 初始化 --- */
    USART_InitTypeDef USART_InitStruct;
    USART_StructInit(&USART_InitStruct);
    USART_InitStruct.USART_BaudRate = 115200;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART2, &USART_InitStruct);

    /* --- NVIC 中断优先级配置 (只开 USART IDLE，不开 RXNE) --- */
    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 6; 
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    /* --- DMA1 Stream5 Channel4 (USART2_RX 硬件映射) 配置 --- */
    DMA_DeInit(DMA1_Stream5);
    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_4;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&USART2->DR; // 外设地址：串口数据寄存器
    DMA_InitStruct.DMA_Memory0BaseAddr = (uint32_t)usart2_rx_dma_buf; // 内存地址：我们的数组
    DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralToMemory;           // 方向：外设到内存
    DMA_InitStruct.DMA_BufferSize = USART2_RX_BUF_SIZE;            // 缓冲大小
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;  // 外设地址不增
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;           // 内存地址递增
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_Mode = DMA_Mode_Circular;                   // 魔法：开启循环模式，永远不会满
    DMA_InitStruct.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_Init(DMA1_Stream5, &DMA_InitStruct);
    DMA_Cmd(DMA1_Stream5, ENABLE); // 启动 RX DMA

    // DMA1 Stream6 Channel4 (USART2_TX 硬件映射) 配置 
    DMA_DeInit(DMA1_Stream6);
    // TX 初始配置与 RX 类似，但不开启循环模式，方向相反
    DMA_InitStruct.DMA_Channel = DMA_Channel_4;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&USART2->DR;
    DMA_InitStruct.DMA_Memory0BaseAddr = (uint32_t)usart2_tx_dma_buf;
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;           // 方向：内存到外设
    DMA_InitStruct.DMA_BufferSize = 0;                             // 初始为 0
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;                     // TX 是单次发送
    DMA_Init(DMA1_Stream6, &DMA_InitStruct);
    // 注意：TX DMA 这里不 ENABLE，发送数据时再启动

    // 2. 开启 USART 的 DMA 触发机制 
    USART_DMACmd(USART2, USART_DMAReq_Rx | USART_DMAReq_Tx, ENABLE);

    // 3. 开启串口与空闲中断 (不再开 RXNE) 
    USART_ITConfig(USART2, USART_IT_IDLE, ENABLE); 
    USART_Cmd(USART2, ENABLE);
}

// 注册一帧数据到达的回调 (仅用于通知，不传数据) 
void usart2_frame_register(usart_receive_callback_t callback)
{
    usart2_frame_callback = callback;
}

// 魔法 1：获取缓冲区中有多少未读数据 
uint16_t usart2_get_buffered_data_len(void)
{
    // DMA_GetCurrDataCounter 返回的是剩余未传输的数量，所以写指针 = 总量 - 剩余量
    uint16_t write_pos = USART2_RX_BUF_SIZE - DMA_GetCurrDataCounter(DMA1_Stream5);
    if (write_pos >= rx_read_pos) {
        return write_pos - rx_read_pos;
    } else {
        return USART2_RX_BUF_SIZE - rx_read_pos + write_pos; // 循环绕回的情况
    }
}

// 魔法 2：ESP32 风格的探针式读取函数 (在你的大循环里无阻塞调用) 
uint16_t usart2_read_bytes(uint8_t *buf, uint16_t max_len)
{
    uint16_t write_pos = USART2_RX_BUF_SIZE - DMA_GetCurrDataCounter(DMA1_Stream5);
    uint16_t len = 0;

    // 当读指针没追上写指针，且还没读够期望长度时，一直搬运数据
    while (rx_read_pos != write_pos && len < max_len) {
        buf[len++] = usart2_rx_dma_buf[rx_read_pos];
        rx_read_pos = (rx_read_pos + 1) % USART2_RX_BUF_SIZE;
    }
    return len;
}

// 魔法 3：零阻塞 DMA 发送，丢给硬件就跑 
void usart2_send_data_dma(const uint8_t *data, uint16_t len)
{
    if (len == 0 || len > USART2_TX_BUF_SIZE) return;

    // 等待上一次 DMA 发送完成 (20Hz 的频率下，这里绝对是秒过，不会阻塞)
    while (DMA_GetCmdStatus(DMA1_Stream6) != DISABLE);

    // 把数据拷贝到 TX DMA 专用的连续内存中
    memcpy(usart2_tx_dma_buf, data, len);

    // 清除 TX DMA 标志位
    DMA_ClearFlag(DMA1_Stream6, DMA_FLAG_TCIF6);

    // 配置发送长度并点火
    DMA_SetCurrDataCounter(DMA1_Stream6, len);
    DMA_Cmd(DMA1_Stream6, ENABLE);
}

// USART2 中断服务函数：只处理 IDLE 断帧 
void USART2_IRQHandler(void)
{
    // 护盾：处理溢出错误
    if (USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET) {
        USART_ReceiveData(USART2); 
    }

    // 核心：空闲中断 (一帧数据接收完毕，线路空闲超过 1 字节时间)
    if (USART_GetITStatus(USART2, USART_IT_IDLE) != RESET) {
        // STM32 清除 IDLE 中断标志的特定连招：先读 SR，再读 DR
        volatile uint32_t temp = USART2->SR;
        temp = USART2->DR;
        (void)temp; // 防止编译器警告

        // 此时数据已经被 DMA 悄悄搬运到了 usart2_rx_dma_buf 里
        // 触发回调，通知主循环“有完整包到了，快来取！”
        if (usart2_frame_callback) {
            usart2_frame_callback(0xFF); // 这里的传参可以随便给，因为真实数据都在缓冲区里
        }
    } 
}