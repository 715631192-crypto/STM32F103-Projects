#include "keypad_input.h"

#include <string.h>

#include "smart_lock_board_config.h"

/*
 * 键位表：行优先。与参考工程 HARDWARE/key_4x4.c 的 s_key_map **完全一致**；
 * 行/列的物理引脚也已与参考工程对齐，绑在硬件层
 * （见 docs/pinout.md 与 smart_lock_pinmap.h）。
 */
static const char keypad_key_map[KEYPAD_ROWS][KEYPAD_COLUMNS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

/* 连续多少次读到同一个键才认为稳定（= 消抖拍数） */
#define KEYPAD_STABLE_SAMPLES 2U

void keypad_scan_init(keypad_scan_t *scan)
{
    if (scan == NULL) {
        return;
    }
    scan->confirmed = KEYPAD_CHARACTER_NONE;
    scan->candidate = KEYPAD_CHARACTER_NONE;
    scan->stable = 0U;
}

/* 采样整张矩阵，返回当前按下的键（无按键返回 '\0'）。 */
static char keypad_sample(const keypad_matrix_io_t *io)
{
    char found = KEYPAD_CHARACTER_NONE;

    if ((io == NULL) || (io->select_row == NULL) || (io->read_column == NULL)) {
        return KEYPAD_CHARACTER_NONE;
    }
    //**扫描每一行**
    //**扫描每一列**
    for (uint8_t row = 0U; row < KEYPAD_ROWS; ++row) {
        io->select_row(io->context, row);
        for (uint8_t column = 0U; column < KEYPAD_COLUMNS; ++column) {
            if (io->read_column(io->context, column)) {
                found = keypad_key_map[row][column];
                /* 继续扫完这一行是没必要的，但必须先把行驱动还原，
                 * 否则最后一行会一直保持低电平。 */
                io->select_row(io->context, (uint8_t)KEYPAD_ROWS);
                return found;
            }
        }
    }
    io->select_row(io->context, (uint8_t)KEYPAD_ROWS); // 取消所有行选中
    return found;
}
// 扫描矩阵，返回当前按下的键（无按键返回 '\0'）。
char keypad_scan_step(const keypad_matrix_io_t *io, keypad_scan_t *scan)
{
    //**一轮完整矩阵扫描，返回当前原始按键字符**。
    const char sample = keypad_sample(io);

    if (scan == NULL) {
        return KEYPAD_CHARACTER_NONE;
    }
    //**消抖计数逻辑**
    if (sample == scan->candidate) {
        if (scan->stable < UINT8_MAX) {
            ++scan->stable;
        }
    
    } else {
        scan->candidate = sample;
        scan->stable = 1U;
    }
    //判断是否达到稳定阈值
    if (scan->stable < KEYPAD_STABLE_SAMPLES) {
        return KEYPAD_CHARACTER_NONE;
    }
    //稳定读到没有按键，确认抬起
    if (scan->candidate == KEYPAD_CHARACTER_NONE) {
        scan->confirmed = KEYPAD_CHARACTER_NONE; /* 已确认抬起 */
        return KEYPAD_CHARACTER_NONE;
    }
    //**长按只上报一次**
    if (scan->confirmed == scan->candidate) {
        return KEYPAD_CHARACTER_NONE; /* 长按只上报一次 */
    }
    //**确认按键**
    scan->confirmed = scan->candidate;
    return scan->confirmed;
}

/* ------------------------------------------------------------------ */
/* 按键序列 → board 输入事件                                            */
/* ------------------------------------------------------------------ */

void keypad_input_init(keypad_input_t *input)
{
    if (input == NULL) {
        return;
    }
    (void)memset(input, 0, sizeof(*input));
    input->totp_mode = false;
    input->log_offset = 0U;
}

uint8_t keypad_input_length(const keypad_input_t *input)
{
    return (input == NULL) ? 0U : input->digit_count;
}

static void keypad_input_clear(keypad_input_t *input)
{
    (void)memset(input->digits, 0, sizeof(input->digits));
    input->digit_count = 0U;
    input->totp_mode = false;
}

/*
 * 追加一位数字。缓冲满时丢弃最旧的一位（FIFO）——虚位密码允许在前面
 * 输入任意乱码，若直接丢弃新输入，用户输入的正确密码会被前缀挤掉。
 */
static void keypad_input_push_digit(keypad_input_t *input, char digit)
{
    if (input->digit_count >= (uint8_t)LOCK_INPUT_MAX_LENGTH) {
        (void)memmove(&input->digits[0], &input->digits[1],
                      (size_t)(LOCK_INPUT_MAX_LENGTH - 1U));
        input->digits[LOCK_INPUT_MAX_LENGTH - 1U] = digit;
        return;
    }
    input->digits[input->digit_count] = digit;
    ++input->digit_count;
}

static bool keypad_input_emit(board_input_type_t type,
                              uint16_t user_id,
                              const char *digits,
                              uint8_t digit_count,
                              board_input_event_t *event)
{
    if (event == NULL) {
        return false;
    }
    (void)memset(event, 0, sizeof(*event));
    event->type = type;
    event->user_id = user_id;
    event->digit_count = digit_count;
    if ((digits != NULL) && (digit_count > 0U)) {
        (void)memcpy(event->digits, digits, digit_count);
    }
    return true;
}
// 处理按键事件，生成 board 输入事件。
bool keypad_input_feed(keypad_input_t *input, char character, uint32_t now_ms,
                       board_input_event_t *event)
{
    if ((input == NULL) || (character == KEYPAD_CHARACTER_NONE)) {
        return false;
    }

    input->last_activity_ms = now_ms;

    if ((character >= '0') && (character <= '9')) {
        keypad_input_push_digit(input, character);// 追加一位数字
        return false;
    }
    //处理特殊按键
    switch (character) {
    case '*':
        /* 缓冲为空时表示"接下来按 TOTP 动态口令处理"；已有输入则当取消键 */
        if (input->digit_count == 0U) {
            input->totp_mode = true;
        } else {
            keypad_input_clear(input);
        }
        return false;
    //确认输入
    case '#': {
        if (input->digit_count == 0U) {
            return false;
        }
        const board_input_type_t type = input->totp_mode ? BOARD_AUTH_TOTP
                                                         : BOARD_AUTH_PIN;
        const bool produced = keypad_input_emit(type, 0U, input->digits,
                                                input->digit_count, event);
        keypad_input_clear(input);
        return produced;
    }
    //查看最近一条日志
    case 'A':
        if (input->digit_count > 0U) {
            keypad_input_clear(input);
        }
        input->log_offset = 1U;
        return keypad_input_emit(BOARD_SHOW_RECENT_LOG, 0U, NULL, 0U, event);
    //继续往前翻一条日志
    case 'B': {
        const uint16_t offset =
            (input->log_offset > UINT16_MAX) ? UINT16_MAX : (uint16_t)input->log_offset;
        if (input->digit_count > 0U) {
            keypad_input_clear(input);
        }
        ++input->log_offset;
        return keypad_input_emit(BOARD_SHOW_RECENT_LOG, offset, NULL, 0U, event);
    }
    //取消输入
       case 'C':
        /*
         * 取消：清缓冲 + 复位日志游标。
         *
         * ★ 这里必须**产出事件**（以前返回 false）。'C' 原先只是悄悄清空，
         * 应用层完全不知道用户按了取消 —— 改主密码向导就靠这个事件退出。
         */
        keypad_input_clear(input);
        input->log_offset = 0U;
        return keypad_input_emit(BOARD_KEYPAD_CANCEL, 0U, NULL, 0U, event);
    //进入管理向导（改主密码）
    case 'D':
        /*
         * 进入管理向导（改主密码）。若手上还有半截数字，先丢掉 —— 否则那些
         * 数字会被当成向导第一步"原密码"的输入，用户会莫名其妙地验证失败。
         */
        if (input->digit_count > 0U) {
            keypad_input_clear(input);
        }
        return keypad_input_emit(BOARD_MENU_REQUEST, 0U, NULL, 0U, event);

    default:
        return false;
    }
}

bool keypad_input_tick(keypad_input_t *input, uint32_t now_ms)
{
    if (input == NULL) {
        return false;
    }
    if ((input->digit_count == 0U) && (!input->totp_mode)) {
        return false;
    }
    if ((now_ms - input->last_activity_ms) <=
        (uint32_t)KEYPAD_INACTIVITY_TIMEOUT_MS) {
        return false;
    }
    keypad_input_clear(input);
    return true;
}
