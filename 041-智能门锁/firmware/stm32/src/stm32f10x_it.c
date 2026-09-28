/*
 * 中断服务函数。
 *
 * 设计原则：中断里**只入队/只清标志**，协议解析一律放到任务里做。
 *
 * 三个例外说明：
 *   - SVC_Handler / PendSV_Handler / SysTick_Handler 不在这里定义。
 *     FreeRTOSConfig.h 把 vPortSVCHandler / xPortPendSVHandler /
 *     xPortSysTickHandler 映射到了这三个向量名，因此它们由 FreeRTOS 的
 *     port.c 定义。这里再写一遍会重复定义。
 *   - 门磁(PC13) / 防撬(PC14) 本轮未启用：PC14 已被 MFRC522 的 RST 占用，
 *     且全片 GPIO 已用满，没有空闲脚给防撬开关。因此不注册 EXTI13 /
 *     EXTI15_10。日后接线时在此补对应 Handler，只做"置标志"即可。
 */

#include "stm32f10x.h"

#include "FreeRTOS.h"
#include "task.h"

#include "usart.h"

#include "board_port.h"

/* ------------------------------------------------------------------ */
/* 三路串口：接收中断只把字节塞进环形缓冲                          */
/* ------------------------------------------------------------------ */
/* USART1 = ESP8266（AT 指令 + 裸 MQTT 字节）
 * USART2 = AS608 / ZW101 指纹模块
 * USART3 = 调试打印（只发不收，但保留中断以防误接） */
void USART1_IRQHandler(void)
{
    usart_rx_irq(USART1);
}

void USART2_IRQHandler(void)
{
    usart_rx_irq(USART2);
}

void USART3_IRQHandler(void)
{
    usart_rx_irq(USART3);
}

/* ------------------------------------------------------------------ */
/* 触摸/按键唤醒：PA8 → EXTI8（下降沿）                              */
/* ------------------------------------------------------------------ */
/* 只清挂起标志并置一个 volatile 待处理标志，真正的"唤醒后刷新界面"由
 * board_poll_input() 在 input 任务里翻译成 BOARD_TOUCH_WAKE 事件再交给
 * access_task。这样中断里不需要碰任何应用状态或 FreeRTOS 对象。 */
void EXTI9_5_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line8) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line8);
        board_wake_irq_notify();
    }
}

/* ------------------------------------------------------------------ */
/* 异常处理：保持最小实现，方便调试器定位                              */
/* ------------------------------------------------------------------ */
void HardFault_Handler(void)
{
    /* 用调试器看 LR/PSP 即可定位；发布版由独立看门狗复位 */
    for (;;) {
    }
}

void MemManage_Handler(void)
{
    for (;;) {
    }
}

void BusFault_Handler(void)
{
    for (;;) {
    }
}

void UsageFault_Handler(void)
{
    for (;;) {
    }
}
