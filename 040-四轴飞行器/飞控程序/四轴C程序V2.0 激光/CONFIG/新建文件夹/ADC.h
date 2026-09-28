#ifndef __ADC_H 
#define __ADC_H
#include "stm32f10x.h"


//#define ADC3_DR_Address    ((u32)0x40013C4C)
//#define ADC1_DR_Address    ((uint32_t)0x4001244C)



//extern u16 ADC_Value[10];
//extern void ADC_DMA_Init(void);
//extern u16 GetADCValue(void);

void ADC1_Init(void);
void Voltage_Check(void);

extern __IO uint16_t ADC_ConvertedValue[];
extern s16 voltage;
#endif 
