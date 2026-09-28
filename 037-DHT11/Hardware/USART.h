#ifndef __USART_H_
#define __USART_H_
#include <stdio.h>
#include <stdarg.h>
#include "stm32f10x.h"
#define  USART_DEBUG USART1
void Usart1_Init(uint32_t baudrate);
void Usart2_Init(uint32_t baudrate);
void Usart_SendByte(USART_TypeDef *USART,uint8_t byte);
void Usart_SendString(USART_TypeDef *USART,unsigned char *str,uint16_t len);
void UsartPrintf(USART_TypeDef *USART,char* format,...);

#endif // __USART_H_
