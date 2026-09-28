#ifndef __H_USART_H_
#define __H_USART_H_
#include "main.h"

#define USART_DEBUG USART1//调试串口1
#define UART_BUF_SIZE 1024  //串口接收缓冲区大小
extern uint8_t uart_receive_buf[UART_BUF_SIZE];
extern u16 uart1_get_ok;
extern uint8_t uart1_mode;
 
//串口开关中断
#define interrupt_open() {__enable_irq();}
#define uart1_open() {USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);}
#define uart1_close() {USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);}
#define uart3_open() {USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);}
#define uart3_close() {USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);}

void uart1_init(uint32_t baudrate);
void uart3_init(uint32_t baudrate);
void Usart_Sendstring(USART_TypeDef *USARTx, uint8_t *str);
void Usart_SendInt(USART_TypeDef *USARTx, int num);
void Usartprintf(USART_TypeDef *USARTx, const char *fmt, ...);
#endif // __H_USART_H_

