#ifndef __TDS_H
#define __TDS_H

#include "sys.h"

void ADC1_Init(void);
float Get_TDS_Value(float temp);
u16 ADC_Read_Test(void);
#endif