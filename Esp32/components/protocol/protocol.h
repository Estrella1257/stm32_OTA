#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// 命令码字典
#define CMD_REPORT_STATE   0x01  // UI 状态包
#define CMD_ENTER_OTA      0xFF  // OTA 升级指令

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

void parse_uart_buffer(uint8_t *data, int len);
void protocol_send_ota_trigger(void);

#endif