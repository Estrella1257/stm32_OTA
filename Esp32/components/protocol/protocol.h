#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// 命令码字典
#define CMD_REPORT_STATE   0x01  // UI 状态包
#define CMD_STM32_ALIVE    0x02
#define CMD_ENTER_BOOT     0x03

#define WEB_SERVER_IP "192.168.201.91"

// 必须加 packed，防止编译器在内存里乱塞空格（字节对齐优化）
typedef struct __attribute__((packed)) {
    uint8_t  header1;     // 必须是 0xAA
    uint8_t  header2;     // 必须是 0x55
    uint8_t  cmd;         // 命令码
    uint8_t  len;         // 有效载荷长度 (这里是 4)
    
    int16_t  speed;       // 速度
    uint8_t  battery;     // 电量
    uint8_t  gear;        // 档位
    
    uint8_t  checksum;    // 校验和
} Frame_UI_State_t;

typedef enum {
    SYS_STATE_IDLE = 0,         // 日常空闲状态
    SYS_STATE_DOWNLOADING,      // 正在从 Web 拉取固件
    SYS_STATE_FLASHING,         // 正在向 STM32 烧录固件
    SYS_STATE_VERIFYING,        // 死亡倒计时中
    SYS_STATE_ROLLBACKING       // 灾难回滚中
} vcu_sys_state_t;

// YModem 协议控制字符字典
#define YM_SOH      0x01  // 128 字节数据包头
#define YM_STX      0x02  // 1024 字节数据包头 (1K-Xmodem)
#define YM_EOT      0x04  // 发送结束
#define YM_ACK      0x06  // 接收成功应答
#define YM_NAK      0x15  // 接收失败重传
#define YM_CAN      0x18  // 取消传输
#define YM_CHAR_C   0x43  // 字母 'C' (请求 CRC16 校验模式)

#define YM_PACKET_SIZE     128
#define YM_1K_PACKET_SIZE  1024

void parse_uart_buffer(uint8_t *data, int len);
void protocol_send_ota_trigger(void);
void protocol_service_start(void);
bool protocol_ymodem_send_file(const char *filepath, const char *filename);

#endif