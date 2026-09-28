#ifndef __RGB_LED_H
#define __RGB_LED_H

#include "stm32f10x.h"

/*
 * RGB 状态指示灯驱动（共阳接法：引脚输出低=亮）
 * ------------------------------------------------
 * 引脚：PA11=红  PA12=绿  PB1=蓝（经限流电阻/三极管驱动，见原理图）
 *
 * 状态约定（非阻塞灯效，rgb_process() 每 10ms 驱动一次）：
 *   验证成功  → 绿色流水跑一圈
 *   验证失败  → 红色整体闪烁 3 次
 *   待机      → 蓝色慢呼吸
 *   报警      → 红色快闪
 *   休眠      → 全灭
 */

typedef enum
{
    RGB_EFFECT_OFF = 0,     /* 全灭             */
    RGB_EFFECT_IDLE,        /* 蓝色慢呼吸       */
    RGB_EFFECT_OK,          /* 绿色流水一圈     */
    RGB_EFFECT_FAIL,        /* 红色闪 3 次      */
    RGB_EFFECT_ALARM,       /* 红色快闪(持续)   */
} RGB_Effect;

void rgb_init(void);
void rgb_set_effect(RGB_Effect e);
void rgb_process(void);          /* 10ms 周期调用 */

#endif
