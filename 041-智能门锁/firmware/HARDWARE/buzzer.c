#include "buzzer.h"
#include "delay.h"

/* 蜂鸣器有效电平：1=高电平响(NPN驱动)  0=低电平响(PNP驱动)
 * 若上电后蜂鸣器一直响，把这里改为 0
 * ★ 2026-09-09 实测上电长鸣 → 模块为低电平触发型，改为 0 */
#define BUZZER_ACTIVE_HIGH  0

/* 引脚：PC15（备份域引脚，速度限制 2MHz，经三极管驱动蜂鸣器足够） */
#if BUZZER_ACTIVE_HIGH
#define BUZZER_ON()   GPIO_SetBits(GPIOC, GPIO_Pin_15)
#define BUZZER_OFF()  GPIO_ResetBits(GPIOC, GPIO_Pin_15)
#else
#define BUZZER_ON()   GPIO_ResetBits(GPIOC, GPIO_Pin_15)
#define BUZZER_OFF()  GPIO_SetBits(GPIOC, GPIO_Pin_15)
#endif

/* 每种模式用一张"节拍表"描述：{-1} 结束
 * 数值 = 该状态的持续时长（单位 10ms）
 * 约定：索引 0 时长内保持初始 OFF，之后奇数索引=响、偶数索引=停 */
static const uint16_t *s_pattern = 0;
static uint8_t  s_idx  = 0;      /* 当前节拍索引   */
static uint32_t s_tlast = 0;     /* 节拍切换时刻   */

/* 各模式节拍表（时长单位 10ms）
 * 索引 0 = 初始停顿时长，之后奇数=响、偶数=停，-1 结束 */
static const uint16_t pat_key[]     = { 5,  5, (uint16_t)-1 };                     /* 短鸣     */
static const uint16_t pat_ok[]      = { 0, 30, (uint16_t)-1 };                       /* 一长300ms*/
static const uint16_t pat_err[]     = { 10, 10, 10, 10, (uint16_t)-1 };             /* 两短     */
static const uint16_t pat_lockout[] = { 8, 8, 8, 8, 8, 8, 8, 8, (uint16_t)-1 };     /* 四短急促 */
static const uint16_t pat_alarm[]   = { 0, 500, (uint16_t)-1 };                      /* 长鸣5s   */

void buzzer_init(void)
{
    GPIO_InitTypeDef g;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    g.GPIO_Pin   = GPIO_Pin_15;
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_2MHz;   /* PC13~PC15 为低速备份域引脚 */
    GPIO_Init(GPIOC, &g);
    BUZZER_OFF();
}

/** @brief 触发一种蜂鸣模式 */
void buzzer_beep(BeepPattern p)
{
    switch (p)
    {
    case BEEP_KEY:     s_pattern = pat_key;     break;
    case BEEP_OK:      s_pattern = pat_ok;      break;
    case BEEP_ERR:     s_pattern = pat_err;     break;
    case BEEP_LOCKOUT: s_pattern = pat_lockout; break;
    case BEEP_ALARM:   s_pattern = pat_alarm;   break;
    default:           s_pattern = 0;           break;
    }
    s_idx   = 0;
    s_tlast = millis();
    BUZZER_OFF();
}

void buzzer_stop(void)
{
    s_pattern = 0;
    BUZZER_OFF();
}

/**
 * @brief 蜂鸣节拍状态机（10ms 调用）
 * 节拍表奇数位=响，偶数位=停，读到 -1 结束
 */
void buzzer_process(void)
{
    uint16_t dur;

    if (s_pattern == 0)
        return;

    dur = s_pattern[s_idx];
    if (dur == (uint16_t)-1)              /* 播完 */
    {
        BUZZER_OFF();
        s_pattern = 0;
        return;
    }

    if ((millis() - s_tlast) >= (uint32_t)dur * 10)   /* 当前节拍时间到 */
    {
        s_tlast = millis();
        s_idx++;
        if (s_pattern[s_idx] == (uint16_t)-1)
            BUZZER_OFF();
        else if (s_idx % 2)              /* 奇数节拍 = 响 */
            BUZZER_ON();
        else                             /* 偶数节拍 = 停 */
            BUZZER_OFF();
    }
}
