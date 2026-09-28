#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f10x.h"

/*
 * SG90 舵机驱动（模拟锁体开/关动作）
 * ------------------------------------------------
 * 引脚：PB0 = TIM3_CH3（PWM 50Hz）
 * 原理：周期 20ms，高电平 0.5~2.5ms 对应 0°~180°
 *       本项目约定： 0° = 上锁位，90° = 开锁位（可按机械结构调整宏）
 */

#define SERVO_LOCK_ANGLE    0     /* 上锁角度   */
#define SERVO_UNLOCK_ANGLE  90    /* 开锁角度   */

void servo_init(void);
void servo_set_angle(uint8_t angle);        /* 0~180° */
void servo_lock(void);                     /* 转到上锁位 */
void servo_unlock(void);                   /* 转到开锁位 */

#endif
