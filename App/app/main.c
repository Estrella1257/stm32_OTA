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
#include "foc.h"
#include "encoder.h"
#include "pid.h"
#include "foc_task.h" 

// 实例化 PI 控制器 (必须在这里分配内存，foc_task.c 里的 extern 才能找到它)
PI_Controller_t motor_speed_pi;

extern float g_vcu_pitch;
extern float g_vcu_roll;
extern int16_t global_sim_speed;
extern volatile uint16_t g_debug_adc_raw;

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

// 需要在某个头文件(比如 timer.h 或 foc_task.h)中声明这个函数，或者在这里直接 extern
extern void TIM6_1ms_Init_And_Enable(void);

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

    // 1. 初始化电机硬件
    Encoder_Init();
    Motor_PWM_Init();
    Motor_Enable();

    // 2. 初始化 PI 控制器 (dt 设为 0.002s，与 TIM6 中的降频节拍对应)
    //PI_Init(&motor_speed_pi, 0.01f, 0.042f, 10.0f); 
    PI_Init(&motor_speed_pi, g_sys_cfg.pid_kp, g_sys_cfg.pid_ki, 10.0f);

    // 3. 强行上电对极 (在此期间电机必须空载)
    printf("\r\n[FOC] System Ready. Starting Alignment...\r\n");
    FOC_Align_Sensor(); 
    
    // 4. 开启 TIM6 1ms 中断，让 FOC 后台接管电机
    TIM6_1ms_Init_And_Enable();

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

            // VOFA+ 专线直达
            float vofa_buf[4];
            vofa_buf[0] = g_vcu_pitch;    
            vofa_buf[1] = g_vcu_roll;     
            vofa_buf[2] = (float)global_sim_speed;     
            vofa_buf[3] = (float)g_debug_adc_raw;             
            VOFA_JustFloat_Send(vofa_buf, 4); 
        }

        // 任务 5：50ms VCU 业务心跳
        if (g_ui_update_flag) {
            g_ui_update_flag = 0;
            
            VCU_Task_50ms(); 

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