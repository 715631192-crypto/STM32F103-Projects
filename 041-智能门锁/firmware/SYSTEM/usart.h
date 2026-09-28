#ifndef __USART_H
#define __USART_H

#include "stm32f10x.h"
#include <stdio.h>

/*
 * 三路串口统一驱动（中断接收 + 环形缓冲，阻塞发送）
 * ---------------------------------------------------------------
 *  USART1  PA9/PA10   115200  → ESP8266 WiFi 模块（AT 指令 + MQTT）
 *  USART2  PA2/PA3     57600  → ZW101/AS608 指纹模块
 *  USART3  PB10/PB11  115200  → 调试打印（printf 重定向）
 *
 *  接收采用"中断写入环形缓冲 + 查询读出"模式，
 *  任何模块都不会在 ISR 里做协议解析（ISR 只入队）。
 */

#define USART1_RX_SIZE  512    /* ESP8266: MQTT 下行报文较长      */
#define USART2_RX_SIZE  256    /* 指纹模块: 应答包最长几十字节     */
#define USART3_RX_SIZE  128    /* 调试口                          */

void     usart1_init(uint32_t baud);
void     usart2_init(uint32_t baud);
void     usart3_init(uint32_t baud);

void     usart_send_bytes (USART_TypeDef *usart, const uint8_t *buf, uint16_t len);
void     usart_send_string(USART_TypeDef *usart, const char *str);
void     usart_send_byte  (USART_TypeDef *usart, uint8_t b);

uint8_t  usart_read_byte (USART_TypeDef *usart, uint8_t *out);          /* 读1字节,成功返回1 */
uint16_t usart_read_bytes(USART_TypeDef *usart, uint8_t *buf, uint16_t max); /* 读出全部可读字节 */
uint16_t usart_rx_available(USART_TypeDef *usart);                       /* 缓冲区内字节数    */
void     usart_rx_clear  (USART_TypeDef *usart);                         /* 清空接收缓冲      */

void     usart_rx_irq(USART_TypeDef *usart);   /* 串口接收中断入口(it.c 调用) */

#endif
