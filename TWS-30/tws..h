#ifndef __TWS_H
#define __TWS_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

// 回调函数类型定义
typedef void (*TWS30_Callback_t)(float ntu, float voltage, uint16_t raw_adc);

// 初始化与配置
void TWS30_Init(void);
void TWS30_ADC_Init(void);

// 数据采集
float TWS30_GetNTU(void);
uint16_t TWS30_GetRawADC(void);
float TWS30_GetVoltage(void);

// 校准功能
void TWS30_CalibrateZero(void);
void TWS30_CalibrateSpan(float standard_ntu);

// 连续监测
void TWS30_StartContinuous(TWS30_Callback_t callback, uint32_t interval_ms);

#endif /* __TWS30_TURBIDITY_H */