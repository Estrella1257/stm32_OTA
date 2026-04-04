#include "main.h"
#include "usart.h"
#include "ringbuffer.h"
#include "protocol.h"
#include "debug_shell.h"
#include "vcu.h"
#include "imu_task.h"
#include "vofa.h"
#include "iwdg.h"
#include "throttle.h"
#include "key.h"

extern float g_vcu_pitch;
extern float g_vcu_roll;
extern int16_t global_sim_speed;

// 外部全局变量
volatile uint8_t g_rx_data = 0;
volatile uint8_t g_rx_flag = 0;
extern volatile uint8_t g_ui_update_flag;
extern volatile uint8_t g_imu_update_flag;

// 环形水池定义
#define RX_POOL_SIZE 2048
uint8_t g_rx_pool[RX_POOL_SIZE];
ringbuffer_t g_rx_ringbuffer;

static void usart1_received(uint8_t data)
{
    if (data == '\r' || data == '\n' || data == ' ') return;
    g_rx_data = data;
    g_rx_flag = 1;
}

int main(void)
{
    System_Reset_State();

    // 1. 底层初始化
    board_lowlevel_init();
    DWT_Delay_Init();
    usart1_init();
    usart1_receive_register(usart1_received);
    usart2_init_dma();
    usart3_init();
    tim3_init();
    IWDG_Init();
    ADC_DMA_Init();
    Key_Init();
    IMU_Task_Init();
    led_init(&led1);
    led_init(&led2);

    // 2. 软件中间件初始化
    ringbuffer_init(&g_rx_ringbuffer, g_rx_pool, RX_POOL_SIZE);
    
    VCU_Config_Init();
	
    Protocol_Send_Alive_Ping();

    // 清理串口标志位并开启中断
    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET) USART_ReceiveData(USART1);
    USART_ClearFlag(USART1, USART_FLAG_RXNE);
    __enable_irq();

    if (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET) 
    {
        printf("[SYS] FATAL -> System recovered from IWDG RESET!\r\n");
        printf("[SYS] FATAL -> A severe crash occurred previously.\r\n");
        
        RCC_ClearFlag(); 
    } 
    else 
    {
        printf("\r\n[SYS] -> Normal Power-On Reset.\r\n");
    }

    // 打印菜单
    DebugShell_ShowMenu();

    // 3. 实时主循环调度
    while (1) 
    {
        // 任务 1：极速倾泻串口数据 
        Protocol_HardwareRx_Poll(&g_rx_ringbuffer);

        // 任务 2：二进制拆包与 ASCII 日志混合解析机
        Protocol_Poll(&g_rx_ringbuffer);

        // 任务 3：处理 PC 调试终端的按键指令
        if (g_rx_flag == 1) {
            g_rx_flag = 0; 
            led_toggle(&led1);
            DebugShell_ProcessCommand((char)g_rx_data);
        }

        // 任务 4：10ms IMU 姿态解算心跳
        if (g_imu_update_flag) {
            g_imu_update_flag = 0;
            IMU_Task_10ms_Update(); 
            Key_Scan_10ms();

            // VOFA+ 专线直达：通过 USART3 扔给电脑，完全不影响 USART1 的打印
            float vofa_buf[4];
            vofa_buf[0] = g_vcu_pitch;    // CH0: 看看车头有没有翘起来        
            vofa_buf[1] = g_vcu_roll;     // CH1: 看看车子有没有摔倒     
            vofa_buf[2] = (float)global_sim_speed;     // CH2: 看看当前车速曲线
            vofa_buf[3] = 0.0f;      // CH3: 留给以后的 PID 目标速度             
            VOFA_JustFloat_Send(vofa_buf, 4); 
        }

        // 任务 5：50ms VCU 业务心跳
        if (g_ui_update_flag) {
            g_ui_update_flag = 0;
            
            VCU_Task_50ms(); // 你的速度、档位逻辑

            IWDG_Feed();
            
            // LED 1Hz 心跳指示灯
            static uint8_t led_time_count = 0;
            if (++led_time_count >= 10) {  
                led_toggle(&led2);
                led_time_count = 0;
            }
        }
    }
}