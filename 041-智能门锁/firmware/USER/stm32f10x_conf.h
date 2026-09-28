#ifndef __STM32F10x_CONF_H
#define __STM32F10x_CONF_H

/* 标准外设库配置头：工程实际用到的外设头文件在此集中包含 */

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_flash.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_bkp.h"
#include "misc.h"

#ifdef  USE_FULL_ASSERT
/* 全断言模式：参数错误时停机并打印错误位置 */
#define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr) ((void)0)
#endif

#endif
