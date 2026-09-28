#include "delay.h"
#include "stm32f10x.h"          /* CoreDebug / DWT / SystemCoreClock 定义 */
#include "FreeRTOS.h"
#include "task.h"

/*
 * 延时实现要点：
 *  - millis() 通过 xTaskGetTickCount() 读 RTOS tick，乘以 portTICK_PERIOD_MS
 *    得到毫秒数；与 RTOS 时基同源，vTaskDelay / vTaskDelayUntil 一致
 *  - delay_ms() 用 vTaskDelay 阻塞，**只能在任务上下文调用**，中断里不能用
 *  - delay_us() 用 Cortex-M3 内置的 DWT->CYCCNT 周期计数器
 *    （72MHz → 1us = 72 cycle），不受中断影响，精度 ±1us
 */

/**
 * @brief 初始化 DWT 周期计数器（必须在 SysTick 之前或之后都行）
 *        Cortex-M3 TRC 章节：DEMCR.TRCENA=1 才能访问 DWT 单元
 */
void delay_init(void)
{
    /* 1. 开启 DWT 外设（DEMCR 寄存器的 TRCENA 位 = bit24） */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* 2. 清零周期计数器 */
    DWT->CYCCNT = 0;

    /* 3. 使能周期计数器 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief 自启动以来的毫秒数（任务上下文安全）
 *        注意：xTaskGetTickCount() 在中断中要用 portTICK_TYPE_IS_ATOMIC 方式，
 *        本工程 32-bit tick，单次读 32 位在 Cortex-M3 上是原子的，故 ISR 也安全
 */
uint32_t millis(void)
{
    /* 调度器未启动时 tick 恒为 0，超时循环会永远等不到期 → 死循环。
     * （实例：main 里 AS608_Check 的 1 秒握手超时）→ 用 DWT 兜底 */
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
        return DWT->CYCCNT / (SystemCoreClock / 1000U);

    return (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/**
 * @brief 毫秒级延时（任务上下文）
 *        FreeRTOS 的 vTaskDelay 会让出 CPU 给同/低优先级任务，
 *        实际睡眠时长 ≥ nms（可能被高优先级任务抢占）
 */
void delay_ms(uint16_t nms)
{
    /* 调度器未启动时（main 里的驱动初始化阶段）vTaskDelay 不可用，
     * 直接调用会挂死 → 退化为 DWT 忙等；任务跑起来后仍走阻塞让出 */
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
    {
        while (nms--)
            delay_us(1000);
        return;
    }

    /* 转换为 tick 数：nms * 1 tick/ms / portTICK_PERIOD_MS */
    vTaskDelay(pdMS_TO_TICKS(nms));
}

/**
 * @brief 微秒级精确延时（轮询 DWT）
 *        用途：I2C 时序、SPI 片选后等待、传感器转换等
 *        不可在中断上下文使用（会一直循环）
 */
void delay_us(uint32_t nus)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = nus * (SystemCoreClock / 1000000U);

    /* 等到 CYCCNT 增加 cycles 个，溢出自然处理（CYCCNT 是 32 位） */
    while ((DWT->CYCCNT - start) < cycles)
        ;
}
