#include "sensor.h"

/* ============================================================
 *  PC13(门磁)/PC14(防撬) 暂未接硬件，以下功能临时禁用：
 *    - sensor_init 不初始化 GPIO/EXTI，避免悬空引脚触发中断
 *    - sensor_door_closed() 固定返回 1（门已关），不触发虚掩报警
 *
 *  ⚠️⚠️ 2026-09-10 变更：**PC14 已被 RC522 占用为 RST 硬复位输出**
 *    （见 mfrc522.c 的 RC522_RST_H/L）。所以下面那段注释**不能再原样取消**，
 *    否则 PC14 会被配成上拉输入，与 RC522 的 RST 输出直接冲突。
 *    若日后要恢复防撬开关，必须先把 RC522 的 RST 挪到别的空闲脚
 *    （PB2 不可用——它是 BOOT1，会破坏串口烧录救援路径），再取消下方注释。
 * ============================================================ */

/**
 * @brief 门磁/防撬 GPIO 初始化（当前为空：PC13/PC14 未接）
 */
void sensor_init(void)
{
    /* --- 硬件未接，暂不初始化 ---
    GPIO_InitTypeDef g;
    EXTI_InitTypeDef e;
    NVIC_InitTypeDef n;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);

    g.GPIO_Pin  = GPIO_Pin_13 | GPIO_Pin_14;
    g.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &g);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource14);
    e.EXTI_Line    = EXTI_Line14;
    e.EXTI_Mode    = EXTI_Mode_Interrupt;
    e.EXTI_Trigger = EXTI_Trigger_Falling;
    e.EXTI_LineCmd = ENABLE;
    EXTI_Init(&e);

    n.NVIC_IRQChannel                   = EXTI15_10_IRQn;
    n.NVIC_IRQChannelPreemptionPriority = 0;
    n.NVIC_IRQChannelSubPriority        = 0;
    n.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&n);
    */
}

/** @brief 读门磁状态：1=门已关（硬件未接，固定返回已关） */
uint8_t sensor_door_closed(void)
{
    return 1;   /* 硬件未接，默认门已关，避免虚掩报警 */
}
