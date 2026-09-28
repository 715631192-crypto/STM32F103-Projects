#ifndef __UART_H__
#define __UART_H__

#include "stdio.h"
#include "stm32f10x.h"



void ANO_Uart1_Init(u32 br_num);
void ANO_Uart1_DeInit(void);
//void ANO_Uart1_IRQ(void);
void ANO_Uart1_Put_Char(unsigned char DataToSend);
void ANO_Uart1_Put_String(unsigned char *Str);
void ANO_Uart1_Put_Buf(unsigned char *DataToSend , u8 data_num);
extern int fputc(int ch, FILE *f);


#endif
