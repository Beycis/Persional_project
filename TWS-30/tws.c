/**
 * @file tws30_turbidity.c
 * @brief TWS-30浊度传感器驱动 - 模拟信号采集
 * @details 适用于STM32F407，使用ADC1_IN0 (PA0)
 */

#include "tws.h"
#include <math.h>

 /* ===================== 硬件配置 ===================== */

 // ADC配置参数
#define TWS30_ADC_CHANNEL       ADC_CHANNEL_0       // PA0 -> ADC1_IN0
#define TWS30_ADC_INSTANCE      ADC1
#define TWS30_GPIO_PORT         GPIOA
#define TWS30_GPIO_PIN          GPIO_PIN_0

// 校准参数（根据实际传感器特性调整）
#define TWS30_VOLTAGE_REF       3.3f                // STM32参考电压
#define TWS30_ADC_RESOLUTION    4096.0f             // 12位ADC
#define TWS30_SENSOR_VMAX       4.5f                // 传感器最大输出电压
#define TWS30_NTU_MAX           1000.0f             // 最大量程

// 滤波参数
#define TWS30_FILTER_SAMPLES    64                  // 滑动平均采样数
#define TWS30_SAMPLE_INTERVAL   50                  // 采样间隔 ms

/* ===================== 数据结构 ===================== */

typedef struct {
    uint16_t raw_buffer[TWS30_FILTER_SAMPLES];
    uint8_t buffer_index;
    uint32_t last_sample_time;
    float current_ntu;
    float current_voltage;
    uint8_t is_calibrated;
    float cal_zero_voltage;     // 零点校准电压（清水）
    float cal_max_voltage;      // 满量程校准电压
} TWS30_HandleTypeDef;

static TWS30_HandleTypeDef tws30_handle = { 0 };
static ADC_HandleTypeDef hadc1;

/* ===================== 底层驱动 ===================== */

/**
 * @brief ADC与GPIO初始化
 */
void TWS30_ADC_Init(void)
{
    // 1. GPIO初始化 (PA0 - 模拟输入)
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    GPIO_InitStruct.Pin = TWS30_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(TWS30_GPIO_PORT, &GPIO_InitStruct);

    // 2. ADC时钟使能
    __HAL_RCC_ADC1_CLK_ENABLE();

    // 3. ADC配置
    hadc1.Instance = TWS30_ADC_INSTANCE;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

    HAL_ADC_Init(&hadc1);

    // 4. ADC通道配置
    ADC_ChannelConfTypeDef sConfig = { 0 };
    sConfig.Channel = TWS30_ADC_CHANNEL;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES; // 长采样时间提高精度
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    // 5. 初始化滤波缓冲区
    for (int i = 0; i < TWS30_FILTER_SAMPLES; i++) {
        tws30_handle.raw_buffer[i] = 0;
    }
    tws30_handle.buffer_index = 0;
    tws30_handle.cal_zero_voltage = 0.0f;  // 默认零点
    tws30_handle.cal_max_voltage = 4.5f;   // 默认满量程
}

/**
 * @brief 单次ADC采集
 * @return 12位ADC原始值 (0-4095)
 */
uint16_t TWS30_ReadRawADC(void)
{
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    uint16_t adc_value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return adc_value;
}

/* ===================== 滤波算法 ===================== */

/**
 * @brief 滑动平均滤波
 * @param new_sample 新采样值
 * @return 滤波后的平均值
 */
uint16_t TWS30_MovingAverage(uint16_t new_sample)
{
    // 替换旧值
    tws30_handle.raw_buffer[tws30_handle.buffer_index] = new_sample;
    tws30_handle.buffer_index = (tws30_handle.buffer_index + 1) % TWS30_FILTER_SAMPLES;

    // 计算平均值（使用32位防止溢出）
    uint32_t sum = 0;
    for (int i = 0; i < TWS30_FILTER_SAMPLES; i++) {
        sum += tws30_handle.raw_buffer[i];
    }
    return (uint16_t)(sum / TWS30_FILTER_SAMPLES);
}

/**
 * @brief 中值滤波（用于剔除异常值）
 * @param samples 采样数组
 * @param n 采样数
 * @return 中值
 */
uint16_t TWS30_MedianFilter(uint16_t* samples, uint8_t n)
{
    // 简单冒泡排序找中值
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (samples[j] > samples[j + 1]) {
                uint16_t temp = samples[j];
                samples[j] = samples[j + 1];
                samples[j + 1] = temp;
            }
        }
    }
    return samples[n / 2];
}

/* ===================== 数据转换 ===================== */

/**
 * @brief ADC值转换为电压
 */
float TWS30_ADCtoVoltage(uint16_t adc_value)
{
    return (float)adc_value * TWS30_VOLTAGE_REF / TWS30_ADC_RESOLUTION;
}

/**
 * @brief 电压转换为浊度值 (NTU)
 * @details 线性映射，支持两点校准
 */
float TWS30_VoltagetoNTU(float voltage)
{
    if (voltage < 0) voltage = 0;
    if (voltage > TWS30_SENSOR_VMAX) voltage = TWS30_SENSOR_VMAX;

    float ntu;

    if (tws30_handle.is_calibrated) {
        // 使用校准参数线性插值
        float ratio = (voltage - tws30_handle.cal_zero_voltage) /
            (tws30_handle.cal_max_voltage - tws30_handle.cal_zero_voltage);
        if (ratio < 0) ratio = 0;
        if (ratio > 1) ratio = 1;
        ntu = ratio * TWS30_NTU_MAX;
    }
    else {
        // 默认线性映射 (0-4.5V -> 0-1000 NTU)
        ntu = (voltage / TWS30_SENSOR_VMAX) * TWS30_NTU_MAX;
    }

    return ntu;
}

/* ===================== 对外接口 ===================== */

/**
 * @brief 初始化TWS30传感器
 */
void TWS30_Init(void)
{
    TWS30_ADC_Init();
    HAL_Delay(100); // 传感器上电稳定时间

    // 预填充滤波缓冲区
    for (int i = 0; i < TWS30_FILTER_SAMPLES; i++) {
        uint16_t raw = TWS30_ReadRawADC();
        tws30_handle.raw_buffer[i] = raw;
        HAL_Delay(5);
    }

    printf("[TWS30] 传感器初始化完成，滤波窗口: %d 点\r\n", TWS30_FILTER_SAMPLES);
}

/**
 * @brief 执行单次测量（带滤波）
 * @return 当前浊度值 (NTU)
 */
float TWS30_GetNTU(void)
{
    uint32_t current_time = HAL_GetTick();

    // 检查采样间隔
    if (current_time - tws30_handle.last_sample_time < TWS30_SAMPLE_INTERVAL) {
        return tws30_handle.current_ntu; // 返回缓存值
    }
    tws30_handle.last_sample_time = current_time;

    // 采集新样本
    uint16_t raw_adc = TWS30_ReadRawADC();

    // 滑动平均滤波
    uint16_t filtered_adc = TWS30_MovingAverage(raw_adc);

    // 转换为电压
    float voltage = TWS30_ADCtoVoltage(filtered_adc);
    tws30_handle.current_voltage = voltage;

    // 转换为浊度
    float ntu = TWS30_VoltagetoNTU(voltage);
    tws30_handle.current_ntu = ntu;

    return ntu;
}

/**
 * @brief 获取原始ADC值（调试用）
 */
uint16_t TWS30_GetRawADC(void)
{
    return TWS30_ReadRawADC();
}

/**
 * @brief 获取当前电压值
 */
float TWS30_GetVoltage(void)
{
    return tws30_handle.current_voltage;
}

/**
 * @brief 零点校准（放入清水中执行）
 * @note 确保传感器完全浸入纯净水中
 */
void TWS30_CalibrateZero(void)
{
    printf("[TWS30] 开始零点校准，请确保传感器在清水中...\r\n");
    HAL_Delay(2000);

    // 采集100次取平均
    uint32_t sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += TWS30_ReadRawADC();
        HAL_Delay(10);
    }
    uint16_t avg_adc = sum / 100;
    tws30_handle.cal_zero_voltage = TWS30_ADCtoVoltage(avg_adc);

    printf("[TWS30] 零点校准完成: %.3fV (ADC: %d)\r\n",
        tws30_handle.cal_zero_voltage, avg_adc);
}

/**
 * @brief 满量程校准（放入标准浊度液中）
 * @param standard_ntu 标准液的已知浊度值
 */
void TWS30_CalibrateSpan(float standard_ntu)
{
    printf("[TWS30] 开始满量程校准，标准液 NTU=%.1f...\r\n", standard_ntu);
    HAL_Delay(2000);

    uint32_t sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += TWS30_ReadRawADC();
        HAL_Delay(10);
    }
    uint16_t avg_adc = sum / 100;
    tws30_handle.cal_max_voltage = TWS30_ADCtoVoltage(avg_adc);
    tws30_handle.is_calibrated = 1;

    printf("[TWS30] 满量程校准完成: %.3fV (ADC: %d)\r\n",
        tws30_handle.cal_max_voltage, avg_adc);
}

/**
 * @brief 连续监测模式（非阻塞）
 * @param callback 数据回调函数
 */
void TWS30_StartContinuous(TWS30_Callback_t callback, uint32_t interval_ms)
{
    printf("[TWS30] 启动连续监测模式，间隔: %d ms\r\n", interval_ms);

    uint32_t last_time = HAL_GetTick();

    while (1) {
        if (HAL_GetTick() - last_time >= interval_ms) {
            last_time = HAL_GetTick();

            float ntu = TWS30_GetNTU();
            float voltage = TWS30_GetVoltage();

            if (callback) {
                callback(ntu, voltage, TWS30_GetRawADC());
            }
        }
        // 可在此处添加其他任务
    }
}

/* ===================== 使用示例 ===================== */

/*
// main.c 示例

#include "tws30_turbidity.h"

void TurbidityCallback(float ntu, float voltage, uint16_t raw_adc)
{
    printf("浊度: %.2f NTU | 电压: %.3f V | ADC: %d\r\n", ntu, voltage, raw_adc);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    UART_Init();  // 初始化串口用于输出

    TWS30_Init();

    // 可选：执行校准
    // TWS30_CalibrateZero();      // 清水校准
    // TWS30_CalibrateSpan(500.0f); // 500NTU标准液校准

    // 方式1：连续监测
    TWS30_StartContinuous(TurbidityCallback, 1000); // 每秒输出一次

    // 方式2：按需读取
    while(1) {
        float ntu = TWS30_GetNTU();
        printf("当前浊度: %.2f NTU\r\n", ntu);
        HAL_Delay(1000);
    }
}
*/