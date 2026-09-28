#include "servo.h"

/**
 * @brief TIM3_CH3(PB0) 输出 50Hz PWM
 * 时钟计算：APB1 定时器时钟 72MHz
 *   预分频 72 → 计数频率 1MHz（1 计数 = 1us）
 *   自动重装 20000 → 周期 20000us = 20ms（50Hz）
 *   占空比：500~2500 计数 = 0.5~2.5ms 高电平
 */
void servo_init(void)
{
    GPIO_InitTypeDef  g;
    TIM_TimeBaseInitTypeDef t;
    TIM_OCInitTypeDef o;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    g.GPIO_Pin   = GPIO_Pin_0;
    g.GPIO_Mode  = GPIO_Mode_AF_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &g);

    t.TIM_Prescaler     = 72 - 1;        /* 1MHz            */
    t.TIM_Period        = 20000 - 1;     /* 20ms 周期       */
    t.TIM_ClockDivision = TIM_CKD_DIV1;
    t.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &t);

    o.TIM_OCMode      = TIM_OCMode_PWM1;             /* CNT<CCR 时输出高 */
    o.TIM_OutputState = TIM_OutputState_Enable;
    o.TIM_Pulse       = 1500;                        /* 初始 90° */
    o.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC3Init(TIM3, &o);                           /* CH3 = PB0 */
    TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);

    servo_lock();                          /* 上电默认上锁 */
}

/**
 * @brief 设置舵机角度（0~180°）
 *        脉宽 = 500us + 角度/180*2000us
 */
void servo_set_angle(uint8_t angle)
{
    uint32_t pulse;
    if (angle > 180) angle = 180;
    pulse = 500 + ((uint32_t)angle * 2000) / 180;   /* 500~2500 */
    TIM_SetCompare3(TIM3, (uint16_t)pulse);
}

/** @brief 转到上锁位 */
void servo_lock(void)   { servo_set_angle(SERVO_LOCK_ANGLE); }

/** @brief 转到开锁位 */
void servo_unlock(void) { servo_set_angle(SERVO_UNLOCK_ANGLE); }
