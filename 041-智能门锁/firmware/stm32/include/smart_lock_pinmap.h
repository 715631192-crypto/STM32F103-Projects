#ifndef SMART_LOCK_PINMAP_H
#define SMART_LOCK_PINMAP_H

#include "stm32f10x.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_gpio.h"

/*
 * STM32F103C8T6（LQFP48）引脚表 —— 与「引脚重排方案图」一一对应。
 *
 * 本文件需要 stm32f10x.h，因此**只有板级适配层**（board_port.c / main.c /
 * stm32f10x_it.c）才包含它。纯可移植的常量在 smart_lock_board_config.h。
 *
 * 硬件外设的脚位是固定的，只有"纯 GPIO 模块"被挪动过（方案图上的红标）：
 *   - 键盘 8 根线**与参考工程 key_4x4.c 完全一致**
 *     （行 PA11/PA12/PA15/PB3，列 PB4/PB5/PB6/PB7）
 *   - 软 I2C 挪到 PB8/PB9
 *   - RGB 红 A0 / 绿 A1 相邻，蓝 B1 不动
 *   - 唤醒键 PA0 → PA8
 *
 * 注意：参考工程的 HARDWARE 驱动里也各自写死了引脚（i2c_soft.c 的 PB8/PB9、
 * mfrc522.c 的 PA4~PA7+PC14、w25q64.c 的 PB12~PB15、servo.c 的 TIM3_CH3/PB0、
 * buzzer.c 的 PC15、rgb_led.c 的 PA0/PA1/PB1、usart.c 的 A9/A10、A2/A3、
 * B10/B11）。本文件是它们的**汇总与核对清单**，并给出 board_port.c 自己要
 * 直接操作的引脚（键盘、唤醒键）。改动硬件接线时两处都要改。
 */

/* ------------------------------------------------------------------ */
/* 键盘 4x4：行=推挽输出（扫描时逐行拉低），列=上拉输入（按下读低）      */
/* ------------------------------------------------------------------ */
/* ★ 2026-09-14：与参考工程 HARDWARE/key_4x4.h 的接线表**完全对齐**：
 *
 *         列1    列2    列3    列4
 *         PB4    PB5    PB6    PB7
 * 行1 PA11 [1]  [2]  [3]  [A]
 * 行2 PA12 [4]  [5]  [6]  [B]
 * 行3 PA15 [7]  [8]  [9]  [C]
 * 行4 PB3  [*]  [0]  [#]  [D]
 *
 * 之前本工程用的是"行列角色互换"的另一套接法（PA15/PB4 当列、PB3/PB5 当行），
 * 与实际接线不符 —— 现场键盘是按参考工程的丝印 R1~R4 / C1~C4 接的。
 * 行序就是扫描顺序，改这里会直接改变 keypad_key_map 的行列归属。 */
#define KEYPAD_ROW1_PORT      GPIOA
#define KEYPAD_ROW1_PIN       GPIO_Pin_11
#define KEYPAD_ROW2_PORT      GPIOA
#define KEYPAD_ROW2_PIN       GPIO_Pin_12
#define KEYPAD_ROW3_PORT      GPIOA
#define KEYPAD_ROW3_PIN       GPIO_Pin_15   /* 原 JTAG，需先 sys_init() 释放 */
#define KEYPAD_ROW4_PORT      GPIOB
#define KEYPAD_ROW4_PIN       GPIO_Pin_3    /* 原 JTAG，需先 sys_init() 释放 */

#define KEYPAD_COL1_PORT      GPIOB
#define KEYPAD_COL1_PIN       GPIO_Pin_4    /* 原 JTAG，需先 sys_init() 释放 */
#define KEYPAD_COL2_PORT      GPIOB
#define KEYPAD_COL2_PIN       GPIO_Pin_5
#define KEYPAD_COL3_PORT      GPIOB
#define KEYPAD_COL3_PIN       GPIO_Pin_6
#define KEYPAD_COL4_PORT      GPIOB
#define KEYPAD_COL4_PIN       GPIO_Pin_7

/* ------------------------------------------------------------------ */
/* 触摸/按键唤醒：PA8，EXTI8，上拉，下降沿唤醒                           */
/* ------------------------------------------------------------------ */
#define WAKE_KEY_PORT         GPIOA
#define WAKE_KEY_PIN          GPIO_Pin_8
#define WAKE_KEY_EXTI_LINE    EXTI_Line8
#define WAKE_KEY_PIN_SOURCE   GPIO_PinSource8
#define WAKE_KEY_IRQn         EXTI9_5_IRQn

/* ------------------------------------------------------------------ */
/* 未被使用 / 已占用的脚位（核对用，禁止再分配）                         */
/* ------------------------------------------------------------------ */
/* PC14 = MFRC522 RST（硬复位输出，由 mfrc522.c 驱动）                */
/* PC13 = 门磁（低有效）—— 本轮**未启用**                            */
/* 其余：PA13/PA14 = SWD；PB2 = BOOT1（保持悬空，勿占用，否则破坏串口救援） */

/* 全片 GPIO 已 100% 占用：PA0~PA15、PB0~PB15、PC13/PC14/PC15 全部有归属，
 * 因此"防撬开关"没有可用引脚，本工程不提供防撬输入。 */

#endif /* SMART_LOCK_PINMAP_H */
