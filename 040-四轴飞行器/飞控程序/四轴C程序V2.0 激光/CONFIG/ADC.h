#ifndef __ADC_H 
#define __ADC_H
#include "stm32f10x.h"

void ADC1_Init(void);
void Voltage_Check(void);

extern __IO uint16_t ADC_ConvertedValue[];
extern s16 voltage;
#endif 


