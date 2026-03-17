#include <stdio.h>
#include "protocol.h"

extern volatile int16_t global_real_speed; 
extern volatile int16_t global_battery;  
extern volatile int16_t global_odo;

// 计算校验和：把前面的所有字节加起来，取低 8 位
static uint8_t calculate_checksum(uint8_t *data, int len) {
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

// 暴力拆包外挂
void parse_uart_buffer(uint8_t *data, int len) {
    // 假设数据包长度固定是 9 个字节 (sizeof(Frame_UI_State_t))
    int frame_size = 9; 

    // 如果收到的数据连一个包的长度都不够，直接扔掉
    if (len < frame_size) return;

    // 像扫描仪一样在收到的数据里找包头 (0xAA 0x55)
    for (int i = 0; i <= len - frame_size; i++) {
        if (data[i] == 0xAA && data[i+1] == 0x55) {
            
            // 找到包头了！直接用 C 语言最霸道的“指针强转”，把这块内存变成结构体！
            Frame_UI_State_t *frame = (Frame_UI_State_t *)&data[i];
            
            // 验证校验和 (计算前 8 个字节的和，比对第 9 个字节)
            uint8_t my_sum = calculate_checksum(&data[i], 8);
            
            if (my_sum == frame->checksum) {
                // 校验通过，数据绝对可靠！开始处理业务
                if (frame->cmd == CMD_REPORT_STATE) {
                    printf("[success] speed: %d km/h, power: %d %%, level: %d\n", 
                            frame->speed, frame->battery, frame->gear);
                    
                    global_real_speed = frame->speed;
                    global_battery = frame->battery;
                }
                
                // 跳过这个已经处理完的包，继续找下一个
                i += (frame_size - 1); 
            } else {
                printf("failed!\n");
            }
        }
    }
}