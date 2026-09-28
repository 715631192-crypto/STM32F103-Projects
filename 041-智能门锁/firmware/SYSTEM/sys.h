#ifndef __SYS_H
#define __SYS_H

#include "stm32f10x.h"

/*
 * 系统级杂项：中断分组、JTAG 引脚释放
 * ------------------------------------------------
 * 本项目外设较多，PA15/PB3/PB4 默认被 JTAG 占用，
 * 需要禁用 JTAG（保留 SWD 调试口）把这 3 个引脚释放给 4x4 矩阵键盘。
 */

void sys_init(void);   /* 中断分组 + AFIO 时钟 + 释放 JTAG 引脚 */

#endif
