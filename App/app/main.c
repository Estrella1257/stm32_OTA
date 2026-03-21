#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "usart.h"
#include "nvs_api.h"        
#include "nvs_types.h"    
#include "ringbuffer.h"  

// 全局变量区
volatile uint8_t g_rx_data = 0;
volatile uint8_t g_rx_flag = 0;

NvsContext_t   g_nvs_ctx;
SystemConfig_t g_sys_cfg;

extern const NvsPort_t stm32_nvs_port;

extern volatile uint8_t g_ui_update_flag;

int16_t global_sim_speed = 0;     
uint8_t global_sim_battery = 100; 
uint8_t global_sim_gear = 0;    
uint8_t global_sim_enable = 0;    

#define RX_POOL_SIZE 2048
uint8_t g_rx_pool[RX_POOL_SIZE];
ringbuffer_t g_rx_ringbuffer;

// 串口中断回调 
static void usart1_received(uint8_t data)
{
    if (data == '\r' || data == '\n' || data == ' ') return;
    g_rx_data = data;
    g_rx_flag = 1;
}

// LED 驱动 
void led(void) {
    led_init(&led1); 
    led_init(&led2); 
    led_init(&led3);
    led_on(&led1);   
    led_on(&led2);   
    led_on(&led3);
}

void ESP32_Send_Dashboard_Data(int16_t speed, uint8_t battery, uint8_t gear) 
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

// 辅助函数：打印当前 NVS 和系统状态
static void Print_System_Status(void) {
    printf("\r\n================ NVS STATUS ================\r\n");
    printf("[NVS] Active Sector : 0x%08lX\r\n", g_nvs_ctx.active_sector_addr);
    printf("[NVS] Standby Sector: 0x%08lX\r\n", g_nvs_ctx.standby_sector_addr);
    printf("[NVS] Generation    : %lu\r\n", g_nvs_ctx.active_generation);
    printf("[NVS] Next Seq ID   : %lu\r\n", g_nvs_ctx.next_seq);
    
    uint32_t used_bytes = g_nvs_ctx.next_write_addr - (g_nvs_ctx.active_sector_addr + sizeof(NvsSectorHeader_t));
    uint32_t total_bytes = NVS_SECTOR_SIZE - sizeof(NvsSectorHeader_t);
    printf("[NVS] Sector Usage  : %lu / %lu Bytes (%.1f%%)\r\n", 
            used_bytes, total_bytes, (float)used_bytes / total_bytes * 100.0f);
            
    printf("------------- APP PAYLOAD --------------\r\n");
    printf("[APP] Odometer      : %lu m\r\n", g_sys_cfg.total_odometer_m);
    printf("[APP] PID Kp        : %.2f\r\n", g_sys_cfg.pid_kp);
    printf("[APP] Speed Limit   : %.1f km/h\r\n", g_sys_cfg.speed_limit_kmh_x10 / 10.0f);
    printf("============================================\r\n\r\n");
}

int main(void)
{
    __disable_irq(); 

    //SysTick (系统滴答定时器) 状态机复位
    SysTick->CTRL = 0;
    //向控制及状态寄存器 (SysTick Control and Status Register) 写入 0
    SysTick->LOAD = 0;
    //清零重装载值寄存器 (SysTick Reload Value Register)
    SysTick->VAL  = 0;
    //清零当前值寄存器 (SysTick Current Value Register)

    //NVIC (嵌套向量中断控制器) 寄存器阵列洗白
    for (int i = 0; i < 8; i++) {
        // ICER (Interrupt Clear-Enable Register): 中断清除使能寄存器
        NVIC->ICER[i] = 0xFFFFFFFF;
        // ICPR (Interrupt Clear-Pending Register): 中断清除挂起寄存器
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    SCB->VTOR = 0x08010000;

    board_lowlevel_init();
    usart1_init();
    usart2_init_dma();
    usart1_receive_register(usart1_received);
    tim3_init();
    ringbuffer_init(&g_rx_ringbuffer, g_rx_pool, RX_POOL_SIZE);
    led();

    // 强杀 ORE 只需要保留在 usart_init 刚配完后的这一次清洗，以及 usart.c 的中断里即可
    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET) {
        USART_ReceiveData(USART1); 
    }
    USART_ClearFlag(USART1, USART_FLAG_RXNE);

    __enable_irq();

    printf("\r\n\r\n[SYS] App Booting at 0x08010000...\r\n");
    printf("[NVS] Initializing NVS subsystem...\r\n");
    
    int ret = nvs_init(&g_nvs_ctx, &stm32_nvs_port);
    if (ret != NVS_OK) {
        printf("[NVS] ERROR: Init failed with code %d!\r\n", ret);
        while(1); 
    }

    ret = nvs_load(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
    if (ret == NVS_OK) {
        printf("[NVS] SUCCESS: Loaded existing configuration!\r\n");
    } else if (ret == NVS_ERR_NOT_FOUND) {
        printf("[NVS] WARNING: No valid config found. Formatting Factory Defaults...\r\n");
        memset(&g_sys_cfg, 0, sizeof(SystemConfig_t));
        g_sys_cfg.total_odometer_m = 0;       
        g_sys_cfg.pid_kp = 1.5f;              
        g_sys_cfg.speed_limit_kmh_x10 = 250;  
        
        nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
    }

    Print_System_Status();
    
    printf(">>> NVS Test Shell Ready <<<\r\n");
    printf("Send '1': Add 100m to Odometer & Save\r\n");
    printf("Send '2': Tune Kp (+0.1) & Save\r\n");
    printf("Send '3': Trigger GC Stress Test (Write 50 times)\r\n");
    printf("Send '4': [NEW] Toggle ESP32 UI Simulator (ON/OFF)\r\n");
    printf("Send 'R': Hard Reboot (Test Data Persistence)\r\n");

    uint8_t esp32_rx_buffer[128];

    while (1) 
    {
        // 第一步：把 DMA 硬件收到的零碎数据，全部倾泻到我们庞大的软件环形缓冲区中 
        uint16_t esp_rx_len = usart2_get_buffered_data_len();
        if (esp_rx_len > 0) {
            uint8_t temp_buf[128]; 
            // 假设单次碎包不会超过128
            if(esp_rx_len > 128) esp_rx_len = 128; 
            
            // 从 DMA 把数据拔出来
            usart2_read_bytes(temp_buf, esp_rx_len);
            // 无脑塞进我们的共用大水池
            ringbuffer_write_block(&g_rx_ringbuffer, temp_buf, esp_rx_len); 
        }

       // 第二步：从水池里捞鱼并按行完整打印 (Line Buffer 模式) 
        static uint8_t line_buf[128];
        static uint16_t line_idx = 0;

        // 只要水池里有数据，就一个字节一个字节地往外抽
        while (ringbuffer_get_length(&g_rx_ringbuffer) > 0) {
            uint8_t ch;
            ringbuffer_read_byte(&g_rx_ringbuffer, &ch);

            // 遇到换行符 (\n)，代表一句话结束了
            if (ch == '\n') {
                line_buf[line_idx] = '\0'; // 封口，变成标准的 C 语言字符串
                
                // 过滤掉可能只有单 \r 或空行的情况
                if (line_idx > 0) { 
                    printf("\r\n[RingBuffer POP] message: %s", line_buf);
                }
                line_idx = 0; // 索引清零，准备迎接下一句话
            } 
            // 遇到回车符 (\r) 直接丢弃，或者当做空格处理，防止串口终端光标错乱
            else if (ch == '\r') {
                continue;
            }
            // 普通可见字符，存入行缓冲区
            else {
                if (line_idx < sizeof(line_buf) - 1) {
                    line_buf[line_idx++] = ch;
                } else {
                    // 防火墙：如果 ESP32 发疯，超过 127 字节都没发换行符，强行截断打印防爆栈
                    line_buf[line_idx] = '\0';
                    printf("\r\n[RingBuffer WARN] too long: %s", line_buf);
                    line_idx = 0;
                }
            }
        }

        if (g_rx_flag == 1)
        {
            char cmd = g_rx_data;
            g_rx_flag = 0; 
            led_toggle(&led1);

            printf("\r\n[DEBUG] Received: '%c' (Hex: 0x%02X)\r\n", 
                   (cmd >= 32 && cmd <= 126) ? cmd : '.', cmd);

            switch (cmd) {
                case '1':
                    g_sys_cfg.total_odometer_m += 100;
                    printf("[*] Riding 100m... Total: %lu m. Saving...\r\n", g_sys_cfg.total_odometer_m);
                    nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
                    break;
                case '2':
                    g_sys_cfg.pid_kp += 0.1f;
                    printf("[*] Tuning Kp -> %.2f. Saving...\r\n", g_sys_cfg.pid_kp);
                    nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
                    break;
                case '3': // 终极 GC 压力测试
                    printf("\r\n[!] STARTING NVS STRESS TEST...\r\n");
                    printf("[!] Pushing 50 records rapidly to trigger Garbage Collection.\r\n");
                    for (int i = 0; i < 50; i++) {
                        g_sys_cfg.total_odometer_m += 1; // 每次加 1 米，制造大量不同数据
                        int save_ret = nvs_save(&g_nvs_ctx, &stm32_nvs_port, &g_sys_cfg);
                        
                        if (save_ret == NVS_OK) {
                            // 打印小点，表示写入成功
                            printf("."); 
                        } else {
                            printf("\r\n[!] SAVE FAILED at iteration %d! Code: %d\r\n", i, save_ret);
                            break;
                        }
                    }
                    printf("\r\n[!] STRESS TEST DONE.\r\n");
                    Print_System_Status(); // 测试完立马打印一次状态，看有没有切扇区
                    break;
                case '4': // 模拟器开关
                    global_sim_enable = !global_sim_enable;
                    printf("\r\n[VCU] ESP32 Cyberpunk Simulator: %s !!!\r\n", global_sim_enable ? "ONLINE" : "💤 OFFLINE");
                    break;
                case 'R':
                case 'r':
                    printf("[!] Resetting...\r\n");
                    NVIC_SystemReset(); 
                    break;
                default:
                    printf("[-] Unknown Command.\r\n");
                    break;
            }
        }

        if (g_ui_update_flag) {
            g_ui_update_flag = 0;
            if (global_sim_enable) {
                    // 速度逻辑
                    global_sim_speed++; 
                    if(global_sim_speed > 85) global_sim_speed = 0;

                    // 电量逻辑
                    static int battery_tick = 0;
                    battery_tick++;
                    if(battery_tick > 10) { 
                        global_sim_battery--;
                        if(global_sim_battery == 0) global_sim_battery = 100;
                        battery_tick = 0;
                    }

                    // 档位逻辑
                    if (global_sim_speed == 0) global_sim_gear = 0; 
                    else if (global_sim_speed < 30) global_sim_gear = 1;
                    else if (global_sim_speed < 60) global_sim_gear = 2;
                    else global_sim_gear = 3; 

                    // 封包并发送
                    ESP32_Send_Dashboard_Data(global_sim_speed, global_sim_battery, global_sim_gear);
            }
            static uint8_t led_time_count = 0;
            led_time_count++;
            // 10 次 50ms = 500ms (即半秒钟翻转一次，1Hz 的完美心跳)
            if (led_time_count >= 10) {  
                led_toggle(&led2);
                led_time_count = 0;
            }
        }
    }
}