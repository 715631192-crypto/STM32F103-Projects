#include "key_4x4.h"

/* 行线（输出）：低电平有效。行1~3 在 GPIOA（A11/A12/A15），行4 在 GPIOB（B3） */
static const uint16_t s_row_pin[4] = { GPIO_Pin_11, GPIO_Pin_12, GPIO_Pin_15, GPIO_Pin_3 };
static GPIO_TypeDef *s_row_port[4] = { GPIOA, GPIOA, GPIOA, GPIOB };
/* 列线（输入上拉）：按下接地为低。4 列全在 GPIOB（B4~B7） */
static const uint16_t s_col_pin[4] = { GPIO_Pin_4, GPIO_Pin_5, GPIO_Pin_6, GPIO_Pin_7 };

/* 键值映射表：key_map[行][列] */
static const char s_key_map[4][4] = {
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' },
};

/* 事件队列（环形） */
static char    s_queue[KEY_EVENT_QUEUE_SIZE];
static uint8_t s_q_head = 0, s_q_tail = 0;

/* 上一次扫描的按键状态（消抖后的稳定态），0=无键 */
static char s_last_key = 0;

/** @brief 把一个按键压入事件队列（满则丢弃） */
static void push_event(char k)
{
    uint8_t next = (s_q_head + 1) % KEY_EVENT_QUEUE_SIZE;
    if (next != s_q_tail)
    {
        s_queue[s_q_head] = k;
        s_q_head = next;
    }
}

/**
 * @brief GPIO 初始化
 * 注意：PA15/PB3/PB4 复用自 JTAG，必须先在 sys_init() 里禁用 JTAG
 */
void key_4x4_init(void)
{
    GPIO_InitTypeDef g;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    /* 行：推挽输出，默认输出高（不选中）。行1~3 在 GPIOA，行4 在 GPIOB */
    g.GPIO_Pin   = GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_15;
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &g);

    g.GPIO_Pin   = GPIO_Pin_3;
    GPIO_Init(GPIOB, &g);

    /* 列：上拉输入（4 列全在 GPIOB：B4~B7） */
    g.GPIO_Pin   = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    g.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &g);

    /* 所有行先输出高（无键按下时的空闲态） */
    GPIO_SetBits(GPIOA, GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_15);
    GPIO_SetBits(GPIOB, GPIO_Pin_3);
}

/**
 * @brief 键盘扫描（每 10ms 调用一次）
 * 原理：逐行拉低，读 4 根列线；某列为低 → 该行该列的键被按下。
 * 消抖：连续两次读到同一键才确认；松开(读到无键)后才接受下一次按下。
 */
void key_scan(void)
{
    uint8_t row, col;
    char cur_key = 0;

    for (row = 0; row < 4 && cur_key == 0; row++)
    {
        /* 当前行拉低，其余行拉高（行1~3 在 GPIOA，行4 在 GPIOB） */
        uint8_t r;
        for (r = 0; r < 4; r++)
        {
            if (r == row) GPIO_ResetBits(s_row_port[r], s_row_pin[r]);
            else          GPIO_SetBits(s_row_port[r], s_row_pin[r]);
        }

        for (col = 0; col < 4; col++)
        {
            /* 读列电平（4 列全在 GPIOB：B4~B7） */
            uint8_t low;
            low = (GPIO_ReadInputDataBit(GPIOB, s_col_pin[col]) == Bit_RESET);

            if (low)
            {
                cur_key = s_key_map[row][col];   /* 找到按下键 */
                break;
            }
        }
    }

    /* 恢复所有行为高电平（降低功耗） */
    GPIO_SetBits(GPIOA, GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_15);
    GPIO_SetBits(GPIOB, GPIO_Pin_3);

    /* 状态转移：无键→有键 = 按下事件（且上次已释放，天然消抖） */
    if (cur_key != 0 && s_last_key == 0)
        push_event(cur_key);
    s_last_key = cur_key;
}

/** @brief 取按键事件，无则返回 0 */
char key_get_event(void)
{
    char k;
    if (s_q_tail == s_q_head)
        return 0;
    k = s_queue[s_q_tail];
    s_q_tail = (s_q_tail + 1) % KEY_EVENT_QUEUE_SIZE;
    return k;
}
