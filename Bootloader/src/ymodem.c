#include <stdio.h>
#include "ymodem.h"
#include "uart.h"
#include "crc16.h"
#include "hal_flash_boot.h"

/**
  * @brief  验证 YModem 数据包
  * @param  pData: 接收缓冲区指针
  * @param  length: 接收到的总长度
  * @param  pPayloadSize: 返回实际的数据区大小 (128 或 1024)
  * @retval 0: 校验成功; -1: 校验失败
  */
int8_t YModem_VerifyPacket(uint8_t *pData, uint16_t length, uint32_t *pPayloadSize)
{
    uint16_t payload_len = 0;
    uint16_t received_crc, calculated_crc;

    // 1. 检查包头
    if (pData[0] == YMODEM_SOH) {
        payload_len = PACKET_SIZE_128;
    } else if (pData[0] == YMODEM_STX) {
        payload_len = PACKET_SIZE_1024;
    } else {
        return -1; 
    }

    // 2. 检查长度防越界 (包头1 + 序号2 + 数据区 + CRC2)
    if (length < (payload_len + PACKET_OVERHEAD)) return -1;

    // 3. 校验包序号 (pData[1] + pData[2] 必须等于 0xFF)
    if ((pData[1] + pData[2]) != 0xFF) return -1; 

    // 4. 提取发送方算好的 CRC
    received_crc = (pData[3 + payload_len] << 8) | pData[3 + payload_len + 1];
    
    // 5. 调用极速查表法 CRC16计算数据区
    calculated_crc = ymodem_crc16(&pData[3], payload_len);

    if (calculated_crc != received_crc) return -1; // 指纹不匹配！

    *pPayloadSize = payload_len;
    return 0; // 完美通过
}

extern uint32_t Get_System_Tick(void); 

int8_t YModem_Receive(void)
{
    YModem_State_t state = YMODEM_STATE_INIT;
    uint32_t flash_ptr = APP_START_ADDR; // 固件存入 APP 区
    uint32_t payload_size = 0;
    uint8_t  expected_packet_num = 0;
    
    uint32_t last_c_time = 0; // 记录上次发 'C' 的时间

    printf("Entering YModem OTA Mode...\r\n");

    while (1)
    {
        // 1. 超时处理与握手请求 (仅在初始化阶段)
        if (state == YMODEM_STATE_INIT)
        {
            // 假设 Get_System_Tick() 返回毫秒。每隔 1000ms 发送一个 'C'
            if (Get_System_Tick() - last_c_time >= 1000) 
            {
                UART2_SendChar(YMODEM_C);
                last_c_time = Get_System_Tick();
            }
        }

        // 2. 硬件前台收到一帧数据！开始状态机跃迁
        if (ymodem_rx_flag == 1)
        {
            ymodem_rx_flag = 0; // 必须第一时间修改标志位，让 DMA 准备接下一包
            printf("\r\n[YMODEM] USART2 DMA RX Triggered! Length: %d bytes\r\n", ymodem_rx_len);

            switch (state)
            {
                // 状态：初始化，等待第 0 包 (包含文件名和大小)
                case YMODEM_STATE_INIT:
                    if (YModem_VerifyPacket(ymodem_rx_buffer, ymodem_rx_len, &payload_size) == 0)
                    {
                        if (ymodem_rx_buffer[1] == expected_packet_num) // 必须是 0 号包
                        {
                            printf("[YMODEM] Pkt#0 OK! Target: %s\r\n", &ymodem_rx_buffer[3]);
                            printf("[FLASH] Preparing App space...\r\n");
                            
                            // 极其重要：在这里擦除 Flash APP 所在的扇区！
                            printf("Erasing Flash sectors for App B...\r\n");
                            hal_flash_erase(0x08010000); // 擦除 Sector 4
                            hal_flash_erase(0x08020000); // 擦除 Sector 5
                            hal_flash_erase(0x08040000); // 擦除 Sector 6
                            hal_flash_erase(0x08060000); // 擦除 Sector 7
                            hal_flash_erase(0x08080000); // 擦除 Sector 8
                            hal_flash_erase(0x080A0000); // 擦除 Sector 9
                            printf("[FLASH] Erase complete. Ready for stream.\r\n");

                            expected_packet_num++; // 下一个期待的是第 1 包
                            UART2_SendChar(YMODEM_ACK);
                            UART2_SendChar(YMODEM_C);   // 告诉上位机：我准备好接收正文了
                            state = YMODEM_STATE_RECEIVING;
                        }
                    }
                    break;

                // 状态：疯狂接收数据包并写入 Flash
                case YMODEM_STATE_RECEIVING:
                    // 如果收到传输结束标志 EOT
                    if (ymodem_rx_buffer[0] == YMODEM_EOT)
                    {
                        printf("[YMODEM] EOT received. Finalizing...\r\n");
                        UART2_SendChar(YMODEM_NAK); // YModem 规定：第一次收到 EOT 必须假装没听清回 NAK
                        state = YMODEM_STATE_END;
                    }
                    // 如果收到正常数据包
                    else if (YModem_VerifyPacket(ymodem_rx_buffer, ymodem_rx_len, &payload_size) == 0)
                    {
                        if (ymodem_rx_buffer[1] == expected_packet_num)
                        {
                            // --- 核心：写入 Flash ---
                            if (hal_flash_write(flash_ptr, &ymodem_rx_buffer[3], payload_size) == HAL_OK)
                            {
                                if (expected_packet_num % 10 == 0) {
                                    printf("[YMODEM] Received Pkt#%d -> Written to 0x%08lX\r\n", expected_packet_num, flash_ptr);
                                }
                                flash_ptr += payload_size;
                                expected_packet_num++;
                                UART2_SendChar(YMODEM_ACK);
                            }
                            else
                            {
                                printf("[ERR] Flash write failed at 0x%08lX!\r\n", flash_ptr);
                                UART2_SendChar(YMODEM_CAN);
                                return -1;
                            }
                        }
                        else if (ymodem_rx_buffer[1] == (uint8_t)(expected_packet_num - 1))
                        {
                            printf("[WRN] Duplicate Pkt#%d detected. Ack resent.\r\n", ymodem_rx_buffer[1]);
                            UART2_SendChar(YMODEM_ACK);
                            // 收到重复包（上位机没收到上一个ACK），只回 ACK，不写 Flash
                            UART2_SendChar(YMODEM_ACK);
                        }
                    }
                    else 
                    {
                        printf("[ERR] Packet #%d Verify Failed! (Len: %d)\r\n", expected_packet_num, ymodem_rx_len);
                        UART2_SendChar(YMODEM_NAK);    
                    }
                    break;

                // 状态：处理结束握手，等待全空包
                case YMODEM_STATE_END:
                    // 第二次收到 EOT
                    if (ymodem_rx_buffer[0] == YMODEM_EOT)
                    {
                        UART2_SendChar(YMODEM_ACK);
                        UART2_SendChar(YMODEM_C); // 索要最后一个空包
                    }
                    // 收到最后的空包 (SOH 开头，数据为空)
                    else if (ymodem_rx_buffer[0] == YMODEM_SOH)
                    {
                        UART2_SendChar(YMODEM_ACK);
                     
                        printf("[OK] Transfer complete. All bytes written.\r\n");
                        return 0; // 成功退出主循环！
                    }
                    break;

                // 处理未在 case 中明确列出的状态，消除警告
                default:
                    break;
            } // end switch
        } // end if (rx_flag == 1)
    } // end while(1)
}