#ifndef __USART2__
#define __USART2__


#include "stm32f10x.h"
#include "WIFI_UFO.h"
#include "stdio.h"
#include "spl06.h"
#include "flow.h"
#include "ALL_DEFINE.h"

void UART2_Init(u32 br_num);
void UART2_IRQ(void);
void UART2_Put_Char(unsigned char DataToSend);
void UART2_Put_String(unsigned char *Str);
void UART2_Put_Buf(unsigned char *DataToSend , u8 data_num); 

#endif
