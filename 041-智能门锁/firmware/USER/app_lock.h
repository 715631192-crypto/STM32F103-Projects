#ifndef __APP_LOCK_H
#define __APP_LOCK_H

#include <stdint.h>

/*
 * 锁体控制模块
 * ================================================================
 * 职责：开锁/上锁动作、自动上锁计时、门磁状态判断（虚掩提醒）
 *
 * 胁迫开锁策略（金融级需求）：
 *   门正常打开（不激怒歹徒），但熄屏 10 秒 + 静默推送最高级别报警。
 */

void app_lock_init(void);
void app_lock_open(uint8_t method, uint16_t user, uint8_t duress);
void app_lock_force_close(void);   /* 云端/菜单强制上锁 */
void app_lock_process(void);       /* 50ms 周期调用     */
uint8_t app_lock_is_open(void);    /* 1=开锁状态        */

#endif
