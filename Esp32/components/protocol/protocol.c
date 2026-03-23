#include <stdio.h>
#include <string.h> 
#include "protocol.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_http_ota.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

extern volatile int16_t global_real_speed; 
extern volatile int16_t global_battery;  
extern volatile int16_t global_gear;  

// 架构师的秘密武器：协议层私有“积攒池”
static uint8_t ring_buf[4096]; 
static int ring_len = 0;       

// OTA 触发全局旗帜 (由 UDP 任务举起，由 UART 任务消费)
volatile bool g_start_ymodem_transfer = false;

//第一部分：底层辅助与工具函数                     
// 1. 发送重启指令给 STM32，让它进入 Bootloader
void protocol_send_ota_trigger(void) 
{
    uint8_t tx_buffer[7];
    uint16_t checksum = 0;

    tx_buffer[0] = 0xAA; 
    tx_buffer[1] = 0x55; 
    tx_buffer[2] = 0x02; // CMD_START_OTA
    tx_buffer[3] = 0x02; 

    tx_buffer[4] = 0x00; 
    tx_buffer[5] = 0xFF; 

    for (int i = 0; i < 6; i++) {
        checksum += tx_buffer[i];
    }
    tx_buffer[6] = (uint8_t)(checksum & 0xFF);

    uart_write_bytes(UART_NUM_0, (const char*)tx_buffer, 7);
}

// 2. YModem 的 CRC16 校验算法
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

// 3. YModem 带超时的单字节接收
static uint8_t ymodem_wait_char(uint32_t timeout_ms) 
{
    uint8_t c = 0;
    int len = uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(timeout_ms));
    return (len > 0) ? c : 0x00;
}

//第二部分：核心业务逻辑函数                       
// 【核心】从硬盘读取固件并使用 YModem 发送给 STM32
bool protocol_ymodem_send_file(const char *filepath, const char *filename) 
{
    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        printf("[YMODEM] ERR: Cannot open file %s\n", filepath);
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    uint32_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t packet[YM_1K_PACKET_SIZE + 5]; 
    uint8_t response;
    
    uart_flush_input(UART_NUM_0);

    // 阶段 1：等 'C'
    do {
        response = ymodem_wait_char(1000); 
    } while (response != YM_CHAR_C);

    // 阶段 2：发第 0 包 (文件名)
    memset(packet, 0, YM_PACKET_SIZE + 5);
    packet[0] = YM_SOH; packet[1] = 0x00; packet[2] = 0xFF;
    strcpy((char*)&packet[3], filename);
    sprintf((char*)&packet[3 + strlen(filename) + 1], "%lu", file_size);
    uint16_t crc = ymodem_crc16(&packet[3], YM_PACKET_SIZE);
    packet[3 + YM_PACKET_SIZE] = (uint8_t)(crc >> 8);
    packet[3 + YM_PACKET_SIZE + 1] = (uint8_t)(crc & 0xFF);
    
    uart_write_bytes(UART_NUM_0, (const char*)packet, YM_PACKET_SIZE + 5);
    if (ymodem_wait_char(15000) != YM_ACK) { fclose(f); return false; }
    if (ymodem_wait_char(15000) != YM_CHAR_C) { fclose(f); return false; }

    // 阶段 3：从文件中 fread 读取发送
    uint32_t offset = 0;
    uint8_t seq = 1;
    
    while (offset < file_size) {
        uint32_t remain = file_size - offset;
        uint16_t chunk_size = (remain >= YM_1K_PACKET_SIZE) ? YM_1K_PACKET_SIZE : YM_PACKET_SIZE;
        
        packet[0] = (chunk_size == YM_1K_PACKET_SIZE) ? YM_STX : YM_SOH;
        packet[1] = seq;
        packet[2] = ~seq;
        
        memset(&packet[3], 0x1A, chunk_size); 
        fread(&packet[3], 1, (remain > chunk_size) ? chunk_size : remain, f);
        
        crc = ymodem_crc16(&packet[3], chunk_size);
        packet[3 + chunk_size] = (uint8_t)(crc >> 8);
        packet[3 + chunk_size + 1] = (uint8_t)(crc & 0xFF);

        uart_write_bytes(UART_NUM_0, (const char*)packet, chunk_size + 5);
        
        response = ymodem_wait_char(2000);
        if (response != YM_ACK) { fclose(f); return false; }
        
        offset += chunk_size;
        seq++;
    }

    // 阶段 4：发送 EOT
    uint8_t eot = YM_EOT;
    uart_write_bytes(UART_NUM_0, (const char*)&eot, 1);
    response = ymodem_wait_char(1000);
    if (response == YM_NAK) {
        uart_write_bytes(UART_NUM_0, (const char*)&eot, 1);
        ymodem_wait_char(1000); 
    }

    // 阶段 5：发送空包收尾
    ymodem_wait_char(1000); 
    memset(packet, 0, YM_PACKET_SIZE + 5);
    packet[0] = YM_SOH; packet[1] = 0x00; packet[2] = 0xFF;
    crc = ymodem_crc16(&packet[3], YM_PACKET_SIZE);
    packet[3 + YM_PACKET_SIZE] = (uint8_t)(crc >> 8);
    packet[3 + YM_PACKET_SIZE + 1] = (uint8_t)(crc & 0xFF);
    
    uart_write_bytes(UART_NUM_0, (const char*)packet, YM_PACKET_SIZE + 5);
    ymodem_wait_char(1000); 
    
    fclose(f); 
    return true;
}

// 【解析】处理从 STM32 发来的日常心跳数据
void parse_uart_buffer(uint8_t *data, int len) {
    if (len <= 0) return;

    if (ring_len + len > sizeof(ring_buf)) {
        ring_len = 0; 
        if (len > sizeof(ring_buf)) return; 
    }

    memcpy(ring_buf + ring_len, data, len);
    ring_len += len;

    int i = 0;
    while (i <= ring_len - 4) {
        if (ring_buf[i] == 0xAA && ring_buf[i+1] == 0x55) {
            uint8_t cmd = ring_buf[i+2];
            uint8_t payload_len = ring_buf[i+3];
            int frame_size = 4 + payload_len + 1; 

            if (i + frame_size > ring_len) break; 

            uint8_t my_sum = 0;
            for(int j = 0; j < frame_size - 1; j++){
                my_sum += ring_buf[i + j];
            }
            
            if (my_sum == ring_buf[i + frame_size - 1]) {
                if (cmd == CMD_REPORT_STATE && payload_len == 4) {
                    global_real_speed = (int16_t)(ring_buf[i+4] | (ring_buf[i+5] << 8));
                    global_battery = ring_buf[i+6];
                    global_gear = ring_buf[i+7];
                } 
                // // 旧时代的眼泪：STM32 主动触发 OTA (已屏蔽) 权力已经移交给 Web 端，防止 STM32 的旧命令造成重复触发！
                // else if (cmd == CMD_OTA_REQUEST) {
                //     printf("\n[PROTOCOL] received stm32 cmd, but Web UDP is the boss now!\n");
                // }

                i += frame_size; 
                continue; 
            }
        }
        i++; 
    }

    int remain = ring_len - i;
    if (remain > 0 && i > 0) {
        memmove(ring_buf, &ring_buf[i], remain);
    }
    ring_len = remain; 
}

// 第三部分：FreeRTOS 任务调度                      
// 【任务 1】负责监听 Web 端发来的 UDP 广播口令
static void udp_listen_task(void *pvParameters) {
    char rx_buffer[128];
    
    while (1) {
        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(8888);
        
        if (bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        printf("[UDP] Listening for Web Commands on port 8888...\n");

        while (1) {
            struct sockaddr_in source_addr;
            socklen_t socklen = sizeof(source_addr);
            // 阻塞等待网页端的魔法广播...
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
            
            if (len > 0) {
                rx_buffer[len] = 0; 
                printf("[UDP] Received broadcast: %s\n", rx_buffer);
                
                if (strncmp(rx_buffer, "OTA_TRIGGER", 11) == 0) {
                    printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
                    printf("[UDP] Web OTA Command Received! Triggering Download...\n");
                    printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
                    
                    // 举起升级大旗！唤醒 UART 任务去干活！
                    g_start_ymodem_transfer = true; 
                }
            }
        }
    }
}

// 【任务 2】负责日常数据解析与 OTA 流程调度
static void uart_protocol_task(void *pvParameters)
{
    uint8_t rx_buf[256];

    while (1) {
        int rx_len = uart_read_bytes(UART_NUM_0, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(10));
        if (rx_len > 0) {
            parse_uart_buffer(rx_buf, rx_len);
        }

        // 捕捉到 UDP 任务举起的旗帜！
        if (g_start_ymodem_transfer == true) {
            g_start_ymodem_transfer = false; 
            
            printf("[SYS] OTA Request received. Starting background download...\n");
            
            // 第一步：偷偷去 Python 服务器下固件
            if (bsp_http_download_firmware() == true) {
                
                printf("[SYS] Download Success! Now triggering STM32 to reboot...\n");
                
                // 第二步：固件已经稳稳躺在 SPIFFS 里了，现在下发指令让 STM32 重启！
                protocol_send_ota_trigger(); 
                
                // 第三步：留出 2 秒钟的时间等待 STM32 进入 Bootloader
                vTaskDelay(pdMS_TO_TICKS(2000));
                
                // 第四步：开始 YModem 传输！
                protocol_ymodem_send_file("/spiffs/app.bin", "app.bin");
                
            } else {
                printf("[SYS] Download Failed! Aborting OTA sequence.\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// 【入口】协议层服务启动函数 (在 main.c 中调用)
void protocol_service_start(void)
{
    // 双核驱动：一边听着 STM32 的日常汇报，一边竖着耳朵听 Web 端的核按钮！
    xTaskCreate(uart_protocol_task, "protocol_task", 4096, NULL, 5, NULL);
    xTaskCreate(udp_listen_task, "udp_listen", 4096, NULL, 5, NULL);
    printf("[PROTOCOL] Service Tasks Started in background.\n");
}