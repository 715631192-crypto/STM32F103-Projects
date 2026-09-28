#include "rgb_led.h"
#include "delay.h"

/* 共阳：输出低电平点亮该通道 */
#define R_ON()  GPIO_ResetBits(GPIOA, GPIO_Pin_0)
#define R_OFF() GPIO_SetBits(GPIOA, GPIO_Pin_0)
#define G_ON()  GPIO_ResetBits(GPIOA, GPIO_Pin_1)
#define G_OFF() GPIO_SetBits(GPIOA, GPIO_Pin_1)
#define B_ON()  GPIO_ResetBits(GPIOB, GPIO_Pin_1)
#define B_OFF() GPIO_SetBits(GPIOB, GPIO_Pin_1)

static RGB_Effect s_effect = RGB_EFFECT_OFF;
static uint8_t    s_step   = 0;      /* 灯效步进计数   */
static uint32_t   s_tlast  = 0;      /* 上次切换毫秒   */

void rgb_init(void)
{
    GPIO_InitTypeDef g;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    g.GPIO_Pin   = GPIO_Pin_0 | GPIO_Pin_1;
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &g);

    g.GPIO_Pin  = GPIO_Pin_1;
    GPIO_Init(GPIOB, &g);

    R_OFF(); G_OFF(); B_OFF();
    rgb_set_effect(RGB_EFFECT_IDLE);   /* 上电进入待机呼吸 */
}

void rgb_set_effect(RGB_Effect e)
{
    s_effect = e;
    s_step   = 0;
    s_tlast  = millis();
    R_OFF(); G_OFF(); B_OFF();         /* 切换先熄灭 */
}

/**
 * @brief 灯效状态机（10ms 调用）
 * 每种灯效用"步骤 + 时间间隔"描述，不阻塞主循环
 */
void rgb_process(void)
{
    uint32_t now = millis();

    switch (s_effect)
    {
    /* ---------- 待机：蓝色慢节律（亮 0.8s / 灭 1.6s，视觉呼吸感） ---------- */
    case RGB_EFFECT_IDLE:
    {
        uint32_t t = (now - s_tlast) % 2400;        /* 2.4s 一个周期 */
        if (t < 800)  B_ON();
        else          B_OFF();
        break;
    }

    /* ---------- 成功：绿色流水一圈（绿→红→蓝依次点亮，循环 2 轮） ---------- */
    case RGB_EFFECT_OK:
    {
        if (now - s_tlast >= 120)                   /* 每 120ms 切一步 */
        {
            s_tlast = now;
            s_step++;
            R_OFF(); G_OFF(); B_OFF();
            if      (s_step % 3 == 1) G_ON();       /* 第1步：绿 */
            else if (s_step % 3 == 2) B_ON();       /* 第2步：蓝 */
            else                      R_ON();       /* 第3步：红 */
            if (s_step >= 6)                          /* 跑完 2 圈 */
            {
                rgb_set_effect(RGB_EFFECT_IDLE);     /* 回到待机呼吸 */
            }
        }
        break;
    }

    /* ---------- 失败：红色整体闪烁 3 次 ---------- */
    case RGB_EFFECT_FAIL:
    {
        if (now - s_tlast >= 180)                   /* 每 180ms 翻转一次 */
        {
            s_tlast = now;
            s_step++;
            if (s_step % 2) { R_ON(); } else { R_OFF(); }
            if (s_step >= 6)                          /* 亮灭 3 次 */
                rgb_set_effect(RGB_EFFECT_IDLE);
        }
        break;
    }

    /* ---------- 报警：红色快闪，直到被清除 ---------- */
    case RGB_EFFECT_ALARM:
    {
        if (now - s_tlast >= 100)
        {
            s_tlast = now;
            s_step++;
            if (s_step % 2) { R_ON(); } else { R_OFF(); }
        }
        break;
    }

    case RGB_EFFECT_OFF:
    default:
        R_OFF(); G_OFF(); B_OFF();
        break;
    }
}
