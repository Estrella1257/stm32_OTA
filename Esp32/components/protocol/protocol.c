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

volatile vcu_sys_state_t g_vcu_state = SYS_STATE_IDLE;
volatile bool g_stm32_alive_ping = false;

//第一部分：底层辅助与工具函数                     
// 1. 发送重启指令给 STM32，让它进入 Bootloader
void protocol_send_ota_trigger(void) 
{
    uint8_t tx_buffer[7];
    uint16_t checksum = 0;

    tx_buffer[0] = 0xAA; 
    tx_buffer[1] = 0x55; 
    tx_buffer[2] = CMD_ENTER_BOOT;
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
    return (len > 0) ? c :0x00;
}

static void report_ota_status(const char* prefix, int value, const char* text_msg) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) return;
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(WEB_SERVER_IP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(8889); // 对应 Python 后台监听的 8889

    char send_buf[128];
    if (prefix[0] == 'P') {
        sprintf(send_buf, "P:%d", value); // 进度条 P:50
    } else {
        sprintf(send_buf, "%s:%s", prefix, text_msg); // 状态 M:下载成功, E:超时错误
    }

    sendto(sock, send_buf, strlen(send_buf), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    close(sock);
}

//第二部分：核心业务逻辑函数                       
// 【核心】从硬盘读取固件并使用 YModem 发送给 STM32

static int last_percent = -1;

bool protocol_ymodem_send_file(const char *filepath, const char *filename) 
{
    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    uint32_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t packet[YM_1K_PACKET_SIZE + 5]; 
    uint8_t response;
    
    // 给 STM32 多留点时间吐开机日志，防止杂音干扰
    vTaskDelay(pdMS_TO_TICKS(1500));
    uart_flush_input(UART_NUM_0);

    // 1：死锁熔断机制与假 C 过滤
    int wait_c_timeout = 0;
    while (1) {
        response = ymodem_wait_char(1000); 
        wait_c_timeout++;
        
        if (wait_c_timeout > 30) {
            fclose(f);
            return false;
        }

        if (response == YM_CHAR_C) {
            uint8_t garbage;
            if (uart_read_bytes(UART_NUM_0, &garbage, 1, pdMS_TO_TICKS(20)) > 0) {
                uart_flush_input(UART_NUM_0);
            } else {
                break; 
            }
        }
    }

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

    // 阶段 3：发送实体数据
    uint32_t offset = 0;
    uint8_t seq = 1;
    
    while (offset < file_size) {
        uint32_t remain = file_size - offset;
        uint16_t chunk_size = (remain >= YM_1K_PACKET_SIZE) ? YM_1K_PACKET_SIZE :YM_PACKET_SIZE;
        
        packet[0] = (chunk_size == YM_1K_PACKET_SIZE) ? YM_STX :YM_SOH;
        packet[1] = seq;
        packet[2] = ~seq;
        
        memset(&packet[3], 0x1A, chunk_size); 
        fread(&packet[3], 1, (remain > chunk_size) ? chunk_size :remain, f);
        
        crc = ymodem_crc16(&packet[3], chunk_size);
        packet[3 + chunk_size] = (uint8_t)(crc >> 8);
        packet[3 + chunk_size + 1] = (uint8_t)(crc & 0xFF);

        // 2：ARQ 单包重传机制 (5条命)
        int retry_count = 0;
        bool chunk_success = false;

        while (retry_count < 5) {
            uart_write_bytes(UART_NUM_0, (const char*)packet, chunk_size + 5);
            response = ymodem_wait_char(2000); 
            
            if (response == YM_ACK) {
                chunk_success = true;
                break; 
            } else if (response == YM_CAN) {
                fclose(f); return false;
            } else {
                retry_count++;
                uart_flush_input(UART_NUM_0); 
            }
        }

        if (chunk_success == false) {
            fclose(f); return false;
        }
        
        offset += chunk_size;
        seq++;

        int percent = (int)((offset * 100) / file_size);
        if (percent != last_percent) { 
            report_ota_status("P", percent, ""); 
            last_percent = percent;
        }
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
    if (g_vcu_state == SYS_STATE_VERIFYING) {
        printf("[RAW RX]:");
        for(int k=0; k<len; k++) printf("%02X ", data[k]);
        printf("\n");
    }

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
                else if (cmd == CMD_STM32_ALIVE) { 
                    if (g_vcu_state == SYS_STATE_VERIFYING) {
                        g_stm32_alive_ping = true;
                    }
                }

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
static void ota_worker_task(void *pvParameters) {
    printf("[OTA] START -> Worker task spawned. Target: DOWNLOAD\n");
    g_vcu_state = SYS_STATE_DOWNLOADING;
    
    if (bsp_http_download_firmware() == true) {
        printf("[OTA] STEP -> 1. HTTP Download complete.\n");
        report_ota_status("M", 0, "HTTP 固件下载完成!开始烧录新版本...");
        
        g_vcu_state = SYS_STATE_FLASHING;
        protocol_send_ota_trigger(); 
        printf("[OTA] STEP -> 2. STM32 Reboot triggered via UART.\n");
        vTaskDelay(pdMS_TO_TICKS(2000));

        if (protocol_ymodem_send_file("/spiffs/app.bin", "app.bin")) {
            printf("[OTA] STEP -> 3. YModem flash complete. Entering 15s VERIFY mode.\n");
            g_vcu_state = SYS_STATE_VERIFYING;
            g_stm32_alive_ping = false;

            int countdown = 15;
            while (countdown > 0) {
                if (g_stm32_alive_ping) break;
                vTaskDelay(pdMS_TO_TICKS(1000));
                countdown--;
                
                char temp_msg[128];
                sprintf(temp_msg, "烧录完毕!等待 STM32 新系统存活心跳 (%d 秒)...", countdown);
                report_ota_status("M", 0, temp_msg);
                printf("[OTA] VERIF -> Waiting for heartbeat... T-%d s\n", countdown);
            }

            if (g_stm32_alive_ping) {
                printf("[OTA] FINAL -> Success! STM32 is alive. Promoting firmware.\n");
                unlink("/spiffs/current.bin"); 
                rename("/spiffs/app.bin", "/spiffs/current.bin");
                report_ota_status("S", 100, "验证成功!新系统已转正并存为备份");
            } else {
                printf("[OTA] FINAL -> FAIL! Heartbeat timeout. Executing ROLLBACK!\n");
                report_ota_status("E", 0, "验证超时!启动灾难回滚...");
                
                struct stat st;
                if (stat("/spiffs/current.bin", &st) == 0) {
                    printf("[OTA] RBK -> Backup current.bin found. Restoring...\n");
                    g_vcu_state = SYS_STATE_ROLLBACKING;
                    protocol_send_ota_trigger(); 
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    
                    if (protocol_ymodem_send_file("/spiffs/current.bin", "current.bin")) {
                        printf("[OTA] RBK -> System restored to previous version.\n");
                        report_ota_status("M", 0, "回滚成功!系统已恢复至备份版本");
                    } else {
                        printf("[OTA] FATAL -> Rollback failed midway!\n");
                        report_ota_status("E", 0, "致命错误：回滚传输过程中中断!");
                    }
                } else {
                    printf("[OTA] FATAL -> No backup firmware found. System BRICKED.\n");
                    report_ota_status("E", 0, "错误：找不到备份固件，无法回滚!");
                }
            }
        } else {
            printf("[OTA] ERR -> YModem handshake/transfer failed.\n");
            report_ota_status("E", 0, "致命错误:YModem 传输失败!");
        }
    } else {
        printf("[OTA] ERR -> HTTP Download failed (Check URL/Server).\n");
        report_ota_status("E", 0, "致命错误:HTTP 下载失败!");
    }

    printf("[OTA] FINAL -> Task Finished. Returning to IDLE.\n");
    g_vcu_state = SYS_STATE_IDLE;
    vTaskDelete(NULL); 
}


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

       printf("[UDP] INIT -> Server bound to port 8888. Listening...\n");

        while (1) {
            struct sockaddr_in source_addr;
            socklen_t socklen = sizeof(source_addr);
            // 阻塞等待网页端的魔法广播...
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
            
            if (len > 0) {
                rx_buffer[len] = 0; 
                printf("[UDP] RECV -> Raw Command: %s\n", rx_buffer);
                
                if (strncmp(rx_buffer, "OTA_TRIGGER", 11) == 0) {
                    if (g_vcu_state == SYS_STATE_IDLE) {
                        printf("[SYS] EXEC -> Starting OTA Worker...\n");
                        // 分配 8192 字节的大栈空间，专门给 HTTP 和文件读写用
                        xTaskCreate(ota_worker_task, "ota_worker", 10240, NULL, 6, NULL);
                    } else {
                        // 防御性编程：如果有人连按两下核按钮，直接拒绝，防止搞崩内存!
                        printf("[SYS] WRN -> OTA task is already running (State: %d). Ignored.\n", g_vcu_state);
                    }
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
        if (g_vcu_state == SYS_STATE_IDLE || g_vcu_state == SYS_STATE_VERIFYING) {
            int rx_len = uart_read_bytes(UART_NUM_0, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(10));
            if (rx_len > 0) {
                parse_uart_buffer(rx_buf, rx_len);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// 【入口】协议层服务启动函数 (在 main.c 中调用)
void protocol_service_start(void)
{
    // 双核驱动：一边听着 STM32 的日常汇报，一边竖着耳朵听 Web 端的核按钮!
    xTaskCreate(uart_protocol_task, "protocol_task", 4096, NULL, 5, NULL);
    xTaskCreate(udp_listen_task, "udp_listen", 4096, NULL, 5, NULL);
    printf("[SYS] EXEC -> Service Tasks Started in background.\n");
}