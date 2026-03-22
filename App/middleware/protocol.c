#include "protocol.h"

// 1. 封装：硬件 DMA 数据吸入
void Protocol_HardwareRx_Poll(ringbuffer_t *rb) 
{
    uint16_t esp_rx_len = usart2_get_buffered_data_len();
    if (esp_rx_len > 0) {
        uint8_t temp_buf[128]; 
        if(esp_rx_len > 128) esp_rx_len = 128; 
        usart2_read_bytes(temp_buf, esp_rx_len);
        ringbuffer_write_block(rb, temp_buf, esp_rx_len); 
    }
}

// 2. 封装：专门处理非二进制的 ASCII 字符串
static void Process_ASCII_Log(uint8_t ch) 
{
    static uint8_t line_buf[128];
    static uint16_t line_idx = 0;

    if (ch == '\n') {
        line_buf[line_idx] = '\0';
        if (line_idx > 0) printf("\r\n[ESP32 LOG] %s", line_buf);
        line_idx = 0;
    } else if (ch == '\r') {
        return; // 忽略回车
    } else {
        if (line_idx < sizeof(line_buf) - 1) {
            line_buf[line_idx++] = ch;
        } else {
            line_buf[line_idx] = '\0';
            printf("\r\n[ESP32 WARN] %s", line_buf);
            line_idx = 0;
        }
    }
}

// 3. 终极解析器：二进制与文本双通状态机
void Protocol_Poll(ringbuffer_t *rb) 
{
    while (ringbuffer_get_length(rb) > 0) 
    {
        // 尝试寻找二进制协议帧头
        if (ringbuffer_get_length(rb) >= 4) 
        {
            uint8_t header1, header2;
            ringbuffer_peek_byte(rb, 0, &header1);
            ringbuffer_peek_byte(rb, 1, &header2);

            // 如果匹配到二进制指令的开头
            if (header1 == 0xAA && header2 == 0x55) 
            {
                uint8_t cmd, len;
                ringbuffer_peek_byte(rb, 2, &cmd);
                ringbuffer_peek_byte(rb, 3, &len);

                uint16_t total_frame_size = 4 + len + 1;

                if (ringbuffer_get_length(rb) < total_frame_size) {
                    break; // 遇到半包，果断退出等下一波数据
                }

                uint8_t frame_buf[32]; 
                ringbuffer_read_block(rb, frame_buf, total_frame_size);

                uint8_t checksum = 0;
                for (int i = 0; i < total_frame_size - 1; i++) {
                    checksum += frame_buf[i];
                }

                if (checksum == frame_buf[total_frame_size - 1]) 
                {
                    // 完美截获一个二进制指令包
                    if (cmd == CMD_START_OTA && len == 2) {
                        if (frame_buf[4] == 0x00 && frame_buf[5] == 0xFF) {
                            printf("\r\n[SYS] OTA Trigger Command Received! Rebooting...\r\n");
                            PWR_BackupAccessCmd(ENABLE);
                            RTC_WriteBackupRegister(RTC_BKP_DR1, 0xAAAA);
                            NVIC_SystemReset();
                        }
                    }
                    continue; // 极其重要：处理完一个二进制包，直接 continue 寻找下一个
                }
            }
        } 
        
        // --- 垃圾回收站（精髓所在） ---
        // 如果程序能走到这里，说明刚才看的第一个字节不是 0xAA，
        // 或者虽然是 0xAA 但校验失败了。说明它是普通的字符串日志！
        // 我们把它从池子里读出来，喂给行缓冲打印器。
        uint8_t ch;
        ringbuffer_read_byte(rb, &ch);
        Process_ASCII_Log(ch);
    }
}

// void Parse_ESP32_Commands(ringbuffer_t *rb) 
// {
//     while (ringbuffer_get_length(rb) >= 4) 
//     {
//         uint8_t header1, header2;
//         ringbuffer_peek_byte(rb, 0, &header1);
//         ringbuffer_peek_byte(rb, 1, &header2);

//         if (header1 == 0xAA && header2 == 0x55) 
//         {
//             uint8_t cmd, len;
//             ringbuffer_peek_byte(rb, 2, &cmd);
//             ringbuffer_peek_byte(rb, 3, &len);

//             uint16_t total_frame_size = 4 + len + 1;

//             if (ringbuffer_get_length(rb) < total_frame_size) {
//                 break; 
//             }

//             uint8_t frame_buf[32]; 
//             ringbuffer_read_block(rb, frame_buf, total_frame_size);

//             uint8_t checksum = 0;
//             for (int i = 0; i < total_frame_size - 1; i++) {
//                 checksum += frame_buf[i];
//             }

//             if (checksum == frame_buf[total_frame_size - 1]) 
//             {
//                 // Valid Frame Received
//                 if (cmd == CMD_START_OTA && len == 2) 
//                 {
//                     if (frame_buf[4] == 0x00 && frame_buf[5] == 0xFF) 
//                     {
//                         printf("\r\n[SYS] OTA Trigger Command Received! Rebooting...\r\n");
                        
//                         PWR_BackupAccessCmd(ENABLE);
//                         RTC_WriteBackupRegister(RTC_BKP_DR1, 0xAAAA);
//                         NVIC_SystemReset();
//                     }
//                 }
//             } 
//             else 
//             {
//                 printf("\r\n[WARN] Checksum error in received frame!\r\n");
//             }
//         } 
//         else 
//         {
//             uint8_t junk;
//             ringbuffer_read_byte(rb, &junk);
//         }
//     }
// }

void Protocol_Send_Telemetry(int16_t speed, uint8_t battery, uint8_t gear)
{
    uint8_t tx_buffer[9];
    uint16_t checksum = 0;

    tx_buffer[0] = 0xAA; // header1
    tx_buffer[1] = 0x55; // header2
    tx_buffer[2] = 0x01; // cmd: CMD_REPORT_STATE
    tx_buffer[3] = 0x04; // len: 速度(2) + 电量(1) + 档位(1) = 4 字节

    // 核心修改：将 int16_t 拆分为两个独立的字节 (小端序)
    tx_buffer[4] = (uint8_t)(speed & 0xFF);         // 速度低 8 位 (Low Byte)
    tx_buffer[5] = (uint8_t)((speed >> 8) & 0xFF);  // 速度高 8 位 (High Byte)
    
    tx_buffer[6] = battery; // 电量
    tx_buffer[7] = gear;    // 档位

    // 计算校验和 (前 8 个字节累加)
    for (int i = 0; i < 8; i++) {
        checksum += tx_buffer[i];
    }
    
    // 取低 8 位作为最终的校验位
    tx_buffer[8] = (uint8_t)(checksum & 0xFF);

    usart2_send_data_dma(tx_buffer, 9);
}