#ifndef SMART_LOCK_KEYPAD_INPUT_H
#define SMART_LOCK_KEYPAD_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "board_port.h"
#include "lock_auth.h"

/*
 * 4x4 矩阵键盘：扫描（可注入 I/O）+ 按键序列到 board 输入事件的翻译。
 *
 * 为什么不直接用参考工程的 HARDWARE/key_4x4.c
 * ------------------------------------------
 * 接线已与参考工程**完全对齐**（见 docs/pinout.md）：
 *   行 = PA11 / PA12 / PA15 / PB3
 *   列 = PB4 / PB5 / PB6 / PB7
 * 键位表也一致。仍然自持一份扫描逻辑，是为了把「选行 / 读列」做成可注入的
 * 函数指针（keypad_matrix_io_t）—— 这样能在**主机上**用一张虚拟矩阵把
 * 消抖、键位映射、按键语义全部测出来，不必上板。
 *
 * 键位表
 *     '1' '2' '3' 'A'
 *     '4' '5' '6' 'B'
 *     '7' '8' '9' 'C'
 *     '*' '0' '#' 'D'
 */

#define KEYPAD_ROWS 4U
#define KEYPAD_COLUMNS 4U
#define KEYPAD_CHARACTER_NONE '\0'

/* 矩阵 I/O 注入点 */
typedef struct {
    void *context;// 上下文指针，用于传递额外信息
    /* 选中某一行：该行输出低电平，其余行输出高电平。
     * 约定 row == KEYPAD_ROWS(4) 表示"取消所有行选中"（全部输出高电平），
     * 扫描结束时一定会被调用一次，驱动实现必须支持这个取值。 */
    void (*select_row)(void *context, uint8_t row);
    /* 读某一列：true = 按下（读到低电平） */
    bool (*read_column)(void *context, uint8_t column);
} keypad_matrix_io_t;

typedef struct {
    char confirmed; /* 已确认按下的键，未按下时为 0 */
    char candidate; /* 本次采样到的键，用于两拍消抖 */
    uint8_t stable; /* 候选键连续命中的次数 */
} keypad_scan_t;

void keypad_scan_init(keypad_scan_t *scan);

/*
 * 采样一次矩阵。返回本次"新按下"的按键字符；无新按键返回 '\0'。
 * 调用周期应等于 KEYPAD_DEBOUNCE_MS 的整数分之一（推荐 10 ms）。
 */
char keypad_scan_step(const keypad_matrix_io_t *io, keypad_scan_t *scan);

/* ------------------------------------------------------------------ */
/* 按键序列 → board 输入事件                                            */
/* ------------------------------------------------------------------ */
/*
 * 按键语义
 *   '0'~'9'  累积到输入缓冲；首字符是 '*' 时进入 TOTP 模式
 *   '*'      仅作为首字符有效，表示"按 TOTP 动态口令处理"
 *   '#'      确认：产出 BOARD_AUTH_PIN 或 BOARD_AUTH_TOTP 事件并清空缓冲
 *   'A'      查看最近日志（offset 0）
 *   'B'      继续往前翻一条（offset 递增）
 *   'C'      取消：清空缓冲、复位日志游标，并产出 BOARD_KEYPAD_CANCEL
 *   'D'      进入管理向导：产出 BOARD_MENU_REQUEST（改主密码见 smart_lock_app.c）
 * 超过 KEYPAD_INACTIVITY_TIMEOUT_MS 无按键则自动清空缓冲（见 keypad_input_tick）。
 */
typedef struct {
    char digits[LOCK_INPUT_MAX_LENGTH];// 输入的数字
    uint8_t digit_count;// 输入的数字数量
    bool totp_mode;// 是否在 TOTP 模式下
    uint32_t last_activity_ms;// 上次按键时间戳
    uint32_t log_offset;// 日志游标
} keypad_input_t;

void keypad_input_init(keypad_input_t *input);

/*
 * 喂入一个按键字符。若该按键触发了一个完整的 board 输入事件，
 * 写入 *event 并返回 true；否则返回 false（*event 内容未定义）。
 */
bool keypad_input_feed(keypad_input_t *input, char character, uint32_t now_ms,
                       board_input_event_t *event);

/* 处理无操作超时：超时后清空输入缓冲。返回是否发生了清空。 */
bool keypad_input_tick(keypad_input_t *input, uint32_t now_ms);

/* 供 OLED 显示用：当前已输入的位数（TOTP 模式下也返回位数） */
uint8_t keypad_input_length(const keypad_input_t *input);

#endif /* SMART_LOCK_KEYPAD_INPUT_H */
