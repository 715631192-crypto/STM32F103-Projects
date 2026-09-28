#ifndef __USART1_H
#define	__USART1_H

#include "stm32f10x.h"
#include <stdio.h>


extern void USART3_SendByte(const int8_t *Data,uint8_t len);

extern void USART3_Config(u32 br_num);
void ANO_UART3_DeInit(void);
void ANO_UART3_IRQ(void);

void USART3_Send1(const int8_t Data);



extern void USART1_setBaudRate(uint32_t baudRate);

#endif /* __USART1_H */

