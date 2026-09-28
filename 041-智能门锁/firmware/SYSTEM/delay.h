#ifndef __DELAY_H
#define __DELAY_H

#include <stdint.h>

/*
 * 延时模块（基于 FreeRTOS 时基）
 * ================================================================
 * - SysTick 中断由 FreeRTOS 接管（1ms tick），本模块不再配置 SysTick
 * - millis()         读 xTaskGetTickCount()，与 RTOS 时基一致
 * - delay_ms()       用 vTaskDelay() 阻塞让出 CPU，可被高优先级任务抢占
 * - delay_us()       用 Cortex-M3 DWT 周期计数器（72MHz），精度 ±1us
 */

uint32_t millis(void);          /* 自启动以来的毫秒数 */
void     delay_ms(uint16_t nms);/* RTOS 阻塞延时，可被抢占 */
void     delay_us(uint32_t nus);/* 微秒级精确延时（轮询 DWT） */
void     delay_init(void);      /* 初始化 DWT（FreeRTOS 启动后再调） */

#endif
