#include <stdio.h>
#include <string.h> 
#include "protocol.h"

extern volatile int16_t global_real_speed; 
extern volatile int16_t global_battery;  
extern volatile int16_t global_gear;  

// 架构师的秘密武器：协议层私有“积攒池”
static uint8_t ring_buf[4096]; // 容量 4096 字节
static int ring_len = 0;       // 池子里目前有多少没解析的数据

// 工业级拆包外挂 (完全免疫内存对齐崩溃)
void parse_uart_buffer(uint8_t *data, int len) {
    int frame_size = 9; 

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
    while (i <= ring_len - frame_size) {
        
        // 抓到包头了！
        if (ring_buf[i] == 0xAA && ring_buf[i+1] == 0x55) {
            
            // 手动计算校验和 (前 8 个字节)
            uint8_t my_sum = 0;
            for(int j = 0; j < 8; j++){
                my_sum += ring_buf[i + j];
            }
            
            // 比对第 9 个字节 (校验位)
            if (my_sum == ring_buf[i+8]) {
                // 校验通过,提取命令码
                uint8_t cmd = ring_buf[i+2];

                if (cmd == CMD_REPORT_STATE) {
                    //   核心修复：抛弃结构体指针强转！
                    //   采用最安全的底层字节拼接，彻底消灭“内存奇数地址异常”
                    int16_t speed = (int16_t)(ring_buf[i+4] | (ring_buf[i+5] << 8));
                    uint8_t battery = ring_buf[i+6];
                    uint8_t gear = ring_buf[i+7];

                    // 赋值给全局变量
                    global_real_speed = speed;
                    global_battery = battery;
                    global_gear = gear;

                    // printf("[success] speed: %d\n", speed); // 测试无误后可以注释掉这行，防止系统卡顿
                }
                
                // 完美吃掉一个包，游标往前大跨步推 9 个字节
                i += frame_size; 
                continue; // 马上回去找下一个包
            }
        }
        
        // 如果当前字节不是包头，或者校验失败，说明是垃圾字节，游标往前挪 1 个字节继续排雷
        i++; 
    }

    // 4. 最伟大的一步：内存搬运 (拯救半包)
    int remain = ring_len - i;
    if (remain > 0) {
        memmove(ring_buf, &ring_buf[i], remain);
    }
    ring_len = remain; // 更新池子剩下的水量，等下一波数据来继续拼
}