#include <stdio.h>
#include <string.h> 
#include "protocol.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern volatile int16_t global_real_speed; 
extern volatile int16_t global_battery;  
extern volatile int16_t global_gear;  

// 架构师的秘密武器：协议层私有“积攒池”
static uint8_t ring_buf[4096]; // 容量 4096 字节
static int ring_len = 0;       // 池子里目前有多少没解析的数据

volatile bool g_start_ymodem_transfer = false;

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
                    // 1. 调用你之前写好的发送函数
                    protocol_send_ota_trigger(); 
                    // 2. 举起旗帜！通知 FreeRTOS 后台任务：“准备发固件啦！”
                    g_start_ymodem_transfer = true;
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

// 内部辅助：标准 XModem CRC16 校验算法
static uint16_t ymodem_crc16(const uint8_t *buf, uint32_t len) 
{
    uint16_t crc = 0;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc = crc << 1;
        }
    }
    return crc;
}

// 内部辅助：带超时的单字节接收
static uint8_t ymodem_wait_char(uint32_t timeout_ms) 
{
    uint8_t c = 0;
    // 使用 ESP-IDF 原生串口读取带 timeout 机制
    int len = uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(timeout_ms));
    return (len > 0) ? c : 0x00;
}

// 核心引擎：YModem 文件发送状态机
bool protocol_ymodem_send_file(const char *filename, const uint8_t *file_data, uint32_t file_size) 
{
    uint8_t packet[YM_1K_PACKET_SIZE + 5]; // 最大包缓存: 头(3) + 数据(1024) + CRC(2)
    uint8_t response;

    uart_flush_input(UART_NUM_0);
    // 阶段 1：等待接收端 (STM32) 喊 'C'
    do {
        response = ymodem_wait_char(1000); // 每一秒听一次
    } while (response != YM_CHAR_C);

    // 阶段 2：发送第 0 包 (文件名和大小)
    memset(packet, 0, YM_PACKET_SIZE + 5);
    packet[0] = YM_SOH; // 第 0 包固定 128 字节
    packet[1] = 0x00;   // Seq = 0
    packet[2] = 0xFF;   // ~Seq = 255

    // 组装文件信息载荷: "filename.bin\0 12345\0"
    int name_len = strlen(filename);
    strcpy((char*)&packet[3], filename);
    sprintf((char*)&packet[3 + name_len + 1], "%lu", file_size);

    uint16_t crc = ymodem_crc16(&packet[3], YM_PACKET_SIZE);
    packet[3 + YM_PACKET_SIZE] = (uint8_t)(crc >> 8);
    packet[3 + YM_PACKET_SIZE + 1] = (uint8_t)(crc & 0xFF);

    uart_write_bytes(UART_NUM_0, (const char*)packet, YM_PACKET_SIZE + 5);

    // 等待 ACK，然后等下一个 'C'
    if (ymodem_wait_char(15000) != YM_ACK) { 
        return false; 
    }
    if (ymodem_wait_char(15000) != YM_CHAR_C) { 
        return false; 
    }

    // 阶段 3：循环发送实体数据包 (使用 1K 模式)
    uint32_t offset = 0;
    uint8_t seq = 1;
    
    while (offset < file_size) {
        uint32_t remain = file_size - offset;
        uint16_t chunk_size = (remain >= YM_1K_PACKET_SIZE) ? YM_1K_PACKET_SIZE : YM_PACKET_SIZE;
        
        packet[0] = (chunk_size == YM_1K_PACKET_SIZE) ? YM_STX : YM_SOH;
        packet[1] = seq;
        packet[2] = ~seq;
        
        // 拷贝数据，不足的部分用 0x1A (Ctrl+Z EOF 标志) 填充
        memset(&packet[3], 0x1A, chunk_size);
        memcpy(&packet[3], file_data + offset, (remain > chunk_size) ? chunk_size : remain);
        
        crc = ymodem_crc16(&packet[3], chunk_size);
        packet[3 + chunk_size] = (uint8_t)(crc >> 8);
        packet[3 + chunk_size + 1] = (uint8_t)(crc & 0xFF);

        // 发送并等待 ACK
        uart_write_bytes(UART_NUM_0, (const char*)packet, chunk_size + 5);
        
        response = ymodem_wait_char(2000);
        if (response != YM_ACK) {
            return false; // 严苛模式：一次失败直接拉倒，等下一次重连
        }
        
        offset += chunk_size;
        seq++;
    }

    // 阶段 4：发送 EOT 结束符
    uint8_t eot = YM_EOT;
    uart_write_bytes(UART_NUM_0, (const char*)&eot, 1);
    
    // YModem 规范：第一个 EOT 通常回 NAK，再发一次 EOT 回 ACK
    response = ymodem_wait_char(1000);
    if (response == YM_NAK) {
        uart_write_bytes(UART_NUM_0, (const char*)&eot, 1);
        ymodem_wait_char(1000); // Wait ACK
    }

    // 阶段 5：收尾，发送空包结束会话
    ymodem_wait_char(1000); // Wait for the final 'C'
    
    memset(packet, 0, YM_PACKET_SIZE + 5);
    packet[0] = YM_SOH;
    packet[1] = 0x00;
    packet[2] = 0xFF;
    crc = ymodem_crc16(&packet[3], YM_PACKET_SIZE);
    packet[3 + YM_PACKET_SIZE] = (uint8_t)(crc >> 8);
    packet[3 + YM_PACKET_SIZE + 1] = (uint8_t)(crc & 0xFF);
    
    uart_write_bytes(UART_NUM_0, (const char*)packet, YM_PACKET_SIZE + 5);
    ymodem_wait_char(1000); // Wait ACK
    
    return true;
}

static void uart_protocol_task(void *pvParameters)
{
    const uint8_t dummy_firmware[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}; 
    uint8_t rx_buf[256];

    while (1) {
        int rx_len = uart_read_bytes(UART_NUM_0, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(10));
        if (rx_len > 0) {
            parse_uart_buffer(rx_buf, rx_len);
        }

        if (g_start_ymodem_transfer == true) {
            g_start_ymodem_transfer = false; 
            // 提醒：未来如果是真正的 WiFi OTA，这里的 dummy_firmware 将会被替换为从云端下载在 RAM 里的真实固件指针
            vTaskDelay(pdMS_TO_TICKS(2000));
            protocol_ymodem_send_file("app_v2.bin", dummy_firmware, sizeof(dummy_firmware));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void protocol_service_start(void)
{
    xTaskCreate(uart_protocol_task, "protocol_task", 4096, NULL, 5, NULL);
    printf("[PROTOCOL] Service Task Started in background.\n");
}