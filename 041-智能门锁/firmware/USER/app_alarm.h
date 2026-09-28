#ifndef __APP_ALARM_H
#define __APP_ALARM_H

#include <stdint.h>

/*
 * 报警模块（防撬 / 胁迫 / 错误锁定 / 门虚掩）
 * ================================================================
 * 分级策略：
 *   防撬        → 本地声光最高强度 + 云端推送（最高级别）
 *   胁迫        → 本地完全静默（不激怒歹徒）+ 云端推送（最高级别）
 *   错误锁定    → 本地声光提示 + 云端推送
 *   门虚掩      → 周期性本地提醒 + 云端推送
 */

/* 报警类型 */
#define ALARM_TAMPER      0    /* 防撬开关触发            */
#define ALARM_DURESS      1    /* 胁迫开锁（静默）        */
#define ALARM_LOCKOUT     2    /* 连续错误锁定            */
#define ALARM_DOOR_AJAR   3    /* 门虚掩提醒              */

#define ALARM_ACTIVE_MS   60000u  /* 声光报警持续时间 */

void app_alarm_init(void);
void app_alarm_tamper_irq(void);      /* 防撬 EXTI 中断入口（只置标志） */
void app_alarm_raise(uint8_t type);   /* 触发/推送一种报警             */
void app_alarm_process(void);         /* 100ms 周期调用                */

#endif
