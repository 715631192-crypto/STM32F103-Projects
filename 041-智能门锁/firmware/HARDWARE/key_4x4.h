#ifndef __KEY_4X4_H
#define __KEY_4X4_H

#include "stm32f10x.h"

/*
 * 4x4 矩阵键盘驱动
 * ------------------------------------------------
 * 行（推挽输出，扫描时逐行拉低）：PA11  PA12  PA15  PB3
 * 列（上拉输入，按下时读到低）：  PB4   PB5   PB6   PB7
 *
 * 键位布局（s_key_map[行][列] 为准）：
 *        列1    列2    列3    列4
 *        PB4    PB5    PB6    PB7
 * 行1  [ 1 ] [ 2 ] [ 3 ] [ A ]   PA11
 * 行2  [ 4 ] [ 5 ] [ 6 ] [ B ]   PA12
 * 行3  [ 7 ] [ 8 ] [ 9 ] [ C ]   PA15
 * 行4  [ * ] [ 0 ] [ # ] [ D ]   PB3
 *
 * A=菜单/确认1  B=取消/返回  C=确认  D=菜单
 * 使用方法：key_scan() 每 10ms 调用一次（内部含消抖），
 *           key_get_event() 从事件队列取出按键字符。
 *
 * ★ 注意（2026-09-09 引脚重排后）：
 *   - PA15/PB3/PB4 复用自 JTAG，已在 sys_init() 里用
 *     GPIO_Remap_SWJ_JTAGDisable 释放，SWD(PA13/14) 保留可下载。
 *   - 键盘板 8 脚排针丝印是 R1~R4 + C1~C4（行列分组标注），
 *     必须按丝印接，不能按插针物理位置直插，否则行序倒置 → 上下镜像。
 *   - 具体接线见《接线总表.md》第六节。
 */

#define KEY_EVENT_QUEUE_SIZE  16

void key_4x4_init(void);
void key_scan(void);                        /* 周期调用(10ms)，含消抖 */
char key_get_event(void);                   /* 返回按键字符，无事件返回0 */

#endif
