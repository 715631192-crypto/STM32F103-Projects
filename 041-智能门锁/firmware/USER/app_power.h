#ifndef __APP_POWER_H
#define __APP_POWER_H

#include <stdint.h>

/*
 * 低功耗管理模块
 * ================================================================
 * 长时间无操作 → STM32 Stop 模式（~14uA，OLED 熄屏、外设停摆）
 * 唤醒源：
 *   PA0 唤醒按键  下降沿（独立触摸按键 / 独立轻触按键接 PA0 与 GND）
 *   PC14 防撬开关 下降沿（休眠中也能触发报警）
 * 唤醒后恢复 72MHz 主频、点亮屏幕。
 *
 * 注意：PA0 EXTI 在 app_power_init() 一次性配置完成，
 *      进 Stop 前不需要再切换。
 */

#define POWER_IDLE_SLEEP_S   120    /* 无操作多少秒后休眠 */

void app_power_init(void);
void app_power_wake_irq(void);        /* 任意 EXTI 唤醒源中断入口（只置标志） */
void app_power_notify_activity(void); /* 任何按键/认证时调用 */
void app_power_process(void);         /* 1s 周期调用 */
uint8_t app_power_is_awake(void);

#endif
