#include <stdio.h>
#include <string.h> 
#include "protocol.h"
#include "driver/uart.h"

extern volatile int16_t global_real_speed; 
extern volatile int16_t global_battery;  
extern volatile int16_t global_gear;  

// 架构师的秘密武器：协议层私有“积攒池”
static uint8_t ring_buf[4096]; // 容量 4096 字节
static int ring_len = 0;       // 池子里目前有多少没解析的数据

#define CMD_OTA_REQUEST  0x03

void parse_uart_buffer(uint8_t *data, int len) {
    if (len <= 0) return;

    // 1. 终极内存防爆墙
    if (ring_len + len > sizeof(ring_buf)) {
        ring_len = 0; 
        if (len > sizeof(ring_buf)) return; // 遇到超级乱码直接丢弃保命
    }

    // 2. 注水：无脑倒进池子的末尾
    memcpy(ring_buf + ring_len, data, len);
    ring_len += len;

    // 3. 捕捞：在积攒池里疯狂寻找完整的数据包
    int i = 0;
    // 核心修改 1：至少需要 4 个字节 (AA 55 CMD LEN) 才能算出整个包有多长
    while (i <= ring_len - 4) {
        
        // 抓到包头了！
        if (ring_buf[i] == 0xAA && ring_buf[i+1] == 0x55) {
            
            uint8_t cmd = ring_buf[i+2];
            uint8_t payload_len = ring_buf[i+3];
            
            // 核心修改 2：动态计算这个包的总长度
            // 总长 = 包头(2) + Cmd(1) + Len(1) + 载荷(payload_len) + 校验和(1)
            int frame_size = 4 + payload_len + 1; 

            // 检查池子里剩下的水量，够不够捞出这一个完整的包？
            if (i + frame_size > ring_len) {
                // 半包！果断 break 跳出 while 循环，等下一波串口中断把数据补齐
                break; 
            }

            // 手动计算校验和 (前 frame_size - 1 个字节相加)
            uint8_t my_sum = 0;
            for(int j = 0; j < frame_size - 1; j++){
                my_sum += ring_buf[i + j];
            }
            
            // 比对最后一个字节 (校验位)
            if (my_sum == ring_buf[i + frame_size - 1]) {
                
                // 校验完美通过，开始根据 CMD 执行业务逻辑
                
                if (cmd == CMD_REPORT_STATE && payload_len == 4) {
                    // 处理 9 字节的遥测数据
                    int16_t speed = (int16_t)(ring_buf[i+4] | (ring_buf[i+5] << 8));
                    uint8_t battery = ring_buf[i+6];
                    uint8_t gear = ring_buf[i+7];

                    global_real_speed = speed;
                    global_battery = battery;
                    global_gear = gear;
                } 
                else if (cmd == CMD_OTA_REQUEST) {
                    // 处理 6 字节的 OTA 请求包
                    printf("\n[PROTOCOL] received stm32 cmd,send ota cmd!\n");
                    // 调用你之前写好的发送函数
                    protocol_send_ota_trigger(); 
                }

                // 完美吃掉一个包，游标往前大跨步推 frame_size 个字节
                i += frame_size; 
                continue; // 马上回去找下一个包
            }
        }
        
        // 如果当前字节不是包头，或者校验失败，说明是垃圾字节，游标往前挪 1 个字节继续排雷
        i++; 
    }

    // 4. 内存搬运 (拯救半包)
    int remain = ring_len - i;
    // 小优化：只有 i > 0 (即确实消费了数据) 时才需要搬内存，省点 CPU 算力
    if (remain > 0 && i > 0) {
        memmove(ring_buf, &ring_buf[i], remain);
    }
    ring_len = remain; // 更新池子剩下的水量
}

void protocol_send_ota_trigger(void) 
{
    uint8_t tx_buffer[7];
    uint16_t checksum = 0;

    tx_buffer[0] = 0xAA; // header1
    tx_buffer[1] = 0x55; // header2
    tx_buffer[2] = 0x02; // cmd: 假设 0x02 就是 CMD_START_OTA
    tx_buffer[3] = 0x02; // len: 数据区长度 2 字节

    tx_buffer[4] = 0x00; // 魔法 payload 1
    tx_buffer[5] = 0xFF; // 魔法 payload 2

    // 计算校验和
    for (int i = 0; i < 6; i++) {
        checksum += tx_buffer[i];
    }
    tx_buffer[6] = (uint8_t)(checksum & 0xFF);

    // 瞬间下发，毫不拖泥带水
    uart_write_bytes(UART_NUM_0, (const char*)tx_buffer, 7);
}