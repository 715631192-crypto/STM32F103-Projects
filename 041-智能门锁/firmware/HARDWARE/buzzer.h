#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"

/*
 * 有源蜂鸣器驱动（PC15，经 NPN 三极管驱动，高电平响）
 * ------------------------------------------------
 * 非阻塞蜂鸣模式：buzzer_process() 每 10ms 驱动一次
 *   BEEP_KEY     按键提示：短鸣 50ms
 *   BEEP_OK      验证成功：一长(300ms)
 *   BEEP_ERR     验证失败：两短
 *   BEEP_LOCKOUT 错误锁定：四短急促
 *   BEEP_ALARM   报警：长鸣 5 秒（防撬）
 */

typedef enum
{
    BEEP_NONE = 0,
    BEEP_KEY,
    BEEP_OK,
    BEEP_ERR,
    BEEP_LOCKOUT,
    BEEP_ALARM,
} BeepPattern;

void buzzer_init(void);
void buzzer_beep(BeepPattern p);
void buzzer_process(void);       /* 10ms 周期调用 */
void buzzer_stop(void);

#endif
