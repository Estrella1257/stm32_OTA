#include "throttle.h"

// 这个数组就是 DMA 的“卸货区”，CPU 随时可以来读，不用管 ADC
volatile uint16_t g_adc_buffer[ADC_FILTER_SIZE]; 

void ADC_DMA_Init(void)
{
    GPIO_InitTypeDef       GPIO_InitStructure;
    ADC_InitTypeDef        ADC_InitStructure;
    ADC_CommonInitTypeDef  ADC_CommonInitStructure;
    DMA_InitTypeDef        DMA_InitStructure;

    // 1. 配置 PA0 为模拟输入 (极其重要：千万别配成浮空输入或下拉)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN; // Analog 模式
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 2. 配置 DMA2_Stream0_Channel0 (ADC1 的专属搬运工)
    DMA_DeInit(DMA2_Stream0);
    DMA_InitStructure.DMA_Channel = DMA_Channel_0;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&(ADC1->DR)); // 源地址：ADC数据寄存器
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)g_adc_buffer;     // 目的地址：你的数组
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;             // 方向：外设到内存
    DMA_InitStructure.DMA_BufferSize = ADC_FILTER_SIZE;                 // 搬运数量：16
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;    // 外设地址不递增 (一直读 DR)
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;             // 内存地址递增 (填满数组)
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; // 16位数据
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;         // 16位数据
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;                     // 循环模式！(填满后自动从头开始)
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA2_Stream0, &DMA_InitStructure);
    DMA_Cmd(DMA2_Stream0, ENABLE); // 启动搬运工

    // 3. 配置 ADC 通用寄存器
    ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4; // 时钟分频
    ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
    ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInit(&ADC_CommonInitStructure);

    // 4. 配置 ADC1
    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b; // 12位精度 (0~4095)
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;          // 只扫一个通道
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;     // 极其重要：开启连续转换！ADC像疯狗一样不断测
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    // 5. 绑定通道：ADC1 的 Channel0 对应 PA0
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_144Cycles);

    // 6. 开启 ADC 的 DMA 请求
    ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);
    ADC_DMACmd(ADC1, ENABLE);

    // 7. 启动 ADC
    ADC_Cmd(ADC1, ENABLE);
    ADC_SoftwareStartConv(ADC1); // 扣下扳机，ADC开始永不停歇地工作
}

// 返回 0 ~ 100 的油门百分比
uint8_t Get_Percent(void)
{
    uint32_t sum = 0;
    uint16_t avg_raw = 0;

    // 1. 均值滤波 (抹平物理震动和电气噪声)
    for (int i = 0; i < ADC_FILTER_SIZE; i++) {
        sum += g_adc_buffer[i];
    }
    avg_raw = sum / ADC_FILTER_SIZE; 
    // 如果是 16 次，这里可以直接写 sum >> 4; 运行极快！

    // 2. 软件死区防呆与映射归一化
    if (avg_raw <= ADC_DEADZONE_MIN) {
        return 0; // 松手状态，或者极其微小的误触，直接归零
    } 
    else if (avg_raw >= ADC_MAX_THROTTLE) {
        return 100; // 狂暴模式，拧到底了
    } 
    else {
        // 核心公式：将 (1100 ~ 3000) 映射到 (0 ~ 100)
        // 公式：(当前值 - 最小值) * 100 / (最大值 - 最小值)
        uint32_t percent = (avg_raw - ADC_DEADZONE_MIN) * 100 / (ADC_MAX_THROTTLE - ADC_DEADZONE_MIN);
        return (uint8_t)percent;
    }
}