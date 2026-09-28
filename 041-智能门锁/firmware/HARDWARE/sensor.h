#ifndef __SENSOR_H
#define __SENSOR_H

#include "stm32f10x.h"

/*
 * 门磁 + 防撬传感器驱动
 * ------------------------------------------------
 * PC13 = 门磁开关（磁敏/干簧管，门关=低电平）
 *        → 用于"门虚掩提醒"与"自动上锁"判断
 * PC14 = 防撬开关（微动/振动开关，被撬=低电平，EXTI 下降沿中断）
 *        → 触发最高优先级防撬报警
 */

void sensor_init(void);            /* GPIO + EXTI 初始化 */
uint8_t sensor_door_closed(void);   /* 1=门已关  0=门开着 */

#endif
