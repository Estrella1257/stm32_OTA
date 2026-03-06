#ifndef __YMODEM_H
#define __YMODEM_H

#include <stdint.h>

/* =========================================================================
 * YModem 协议控制字符 (魔法暗号)
 * ========================================================================= */
#define YMODEM_SOH      0x01  // Start Of Header: 标志这是一个 128 字节的数据包
#define YMODEM_STX      0x02  // Start Of Text:   标志这是一个 1024 字节的数据包
#define YMODEM_EOT      0x04  // End Of Transmission: 发送方说“我发完了”
#define YMODEM_ACK      0x06  // Acknowledge: 接收方回复“包收到了且校验无误”
#define YMODEM_NAK      0x15  // Negative Acknowledge: 接收方回复“包有错，请重发”
#define YMODEM_CAN      0x18  // Cancel: 任何一方想强行中止传输 (通常连发两个 CAN)
#define YMODEM_C        0x43  // 字母 'C': 接收方发出的握手信号，要求“开始传输，并使用 CRC16 校验”

/* =========================================================================
 * YModem 数据包尺寸定义
 * ========================================================================= */
#define PACKET_SIZE_128     128   // SOH 包的数据净荷大小
#define PACKET_SIZE_1024    1024  // STX 包的数据净荷大小

#define PACKET_HEADER_SIZE  3     // 包头大小: 控制字符(1) + 包序号(1) + 序号反码(1)
#define PACKET_TRAILER_SIZE 2     // 包尾大小: CRC16 校验码(2)
#define PACKET_OVERHEAD     (PACKET_HEADER_SIZE + PACKET_TRAILER_SIZE) // 额外开销(5字节)

// 接收缓冲区的最大容量 = 1024(最大净荷) + 5(额外开销) = 1029 字节
// 建议分配 1030 字节以上以防溢出
#define YMODEM_RX_BUFFER_SIZE  1032  

/* =========================================================================
 * YModem 状态机状态定义
 * ========================================================================= */
typedef enum {
    YMODEM_STATE_INIT = 0,      // 初始状态，不断发送 'C' 请求连接
    YMODEM_STATE_WAIT_INFO,     // 等待第 0 包 (包含文件名和文件大小)
    YMODEM_STATE_RECEIVING,     // 等待并接收正式的数据包 (第 1, 2, 3...包)
    YMODEM_STATE_END,           // 收到第一个 EOT，处理结束握手
    YMODEM_STATE_COMPLETE,      // 彻底完成，准备跳转
    YMODEM_STATE_ERROR          // 出现严重错误或被 CAN 中止
} YModem_State_t;

int8_t YModem_VerifyPacket(uint8_t *pData, uint16_t length, uint32_t *pPayloadSize);
int8_t YModem_Receive(void);

#endif /* __YMODEM_H */