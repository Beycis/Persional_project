#include "TDS.h"
#include "sys.h"
#include "delay.h"
#include <stdbool.h>

// PA4 对应 ADC1 通道4
void ADC1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef  ADC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    // PA4 模拟输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);
}

// 稳定读取ADC
u16 ADC_Read(void)
{
    ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 1, ADC_SampleTime_144Cycles);
    ADC_SoftwareStartConv(ADC1);
    while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == 0);
    u16 val = ADC_GetConversionValue(ADC1);
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    return val;
}

// ==============================
// 【最终精准版】自来水 50~120
// ==============================
float Get_TDS_Value(float temp)
{
    // 没有温度传感器就先传25.0
    // （你也可以在函数外保证 temp 合法）
    if (temp < 0.0f) temp = 25.0f;

    // 1) 多次采样取平均（减少噪声）
    uint32_t sum = 0;
    const int N = 15;

    for (int i = 0; i < N; i++)
    {
        sum += ADC_Read();     // 需要保证 ADC_Read() 返回的是 0~4095
        delay_ms(2);
    }

    float adc = (float)sum / (float)N;   // 平均后的ADC值（0~4095）

    // 2) ADC 转电压（假设参考电压 3.3V，12bit -> 4095）
    const float VREF = 3.3f;
    const float ADC_MAX = 4095.0f;

    float voltage = (adc * VREF) / ADC_MAX;  // V

    // 3) TDS（ppm）与电压的关系
    // 说明：廉价TDS模块常见换算是“线性近似”或“经验多项式”。
    // 你原来用：tds = vol * 33
    // 但严格来说你需要确认“vol”的单位确实是模块推导使用的电压。
    //
    // 经验上很多人用：
    //    TDS(ppm) = (133.42*V^3 - 255.86*V^2 + 857.39*V) * 0.5
    // 但不同模块/标定差异很大。
    //
    // 如果你之前就已经跑通大方向（比如大致落在100~600ppm），
    // 那就保留线性系数33，但我会把温补和限制做成“可关闭”的形式。

    float tds = voltage * 33.0f;  // 线性近似（按你原思路保留）

    // 4) 温度补偿（常见做法：TDS/EC 随温度按比例变化）
    // 你的写法：tds = tds / (1 + 0.008*(temp-25))
    // 这个形式有些资料用在 EC 上，方向也可能相反（取决于你使用的是 EC 还是 TDS、以及系数定义）。
    //
    // 我给一个更常见且更不容易“反着”的版本：
    // 通常：EC25 = EC / (1 + 0.019*(T-25))
    // 或：EC25 = EC * 100/(T+...); 但最常见的是0.019。
    //
    // 既然你原系数用0.008，我就先沿用0.008，避免你现有标定完全翻车。
    float k = 1.0f + 0.008f * (temp - 25.0f);
    if (k > 0.0001f) tds = tds / k;

    // 5) （重要）不要硬限制在30~300，否则“数值永远不对”
    // 如果你担心异常，可以只做“温和裁剪”，比如 tds<0 ->0，tds>2000 -> 2000（按你实际需求）
    if (tds < 0.0f) tds = 0.0f;
    // if (tds > 2000.0f) tds = 2000.0f;  // 你要的话可以打开

    return tds;
}
