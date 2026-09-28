#ifndef __STM32F10x_IT_H
#define __STM32F10x_IT_H

#include "stm32f10x.h"

/*
 * FreeRTOS 已接管以下中断（由 portable/Keil|RVDS\ARM_CM3\port.c 提供）：
 *   SVC_Handler   - vPortSVCHandler（FreeRTOS 系统调用入口）
 *   PendSV_Handler - xPortPendSVHandler（任务上下文切换）
 *   SysTick_Handler - xPortSysTickHandler（系统节拍）
 * 本文件不要再定义，否则会链接冲突。
 *
 * 启动文件 startup_stm32f103xx.s 已用 weak 声明 NMI / HardFault 等，
 * 这里只声明用户实现的中断函数（EXTI + USART）。
 */

#endif
