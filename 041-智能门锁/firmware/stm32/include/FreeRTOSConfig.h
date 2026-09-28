#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*
 * FreeRTOS 配置（STM32F103C8T6，Cortex-M3，72 MHz）
 *
 * 基准来自参考工程 USER/FreeRTOSConfig.h，改动只有三处，都是本工程的硬需求：
 *
 *   1. configSUPPORT_STATIC_ALLOCATION = 1
 *      smart_lock_app.c 全程使用 xQueueCreateStatic / xTaskCreateStatic，
 *      不开静态分配会直接编译不过。对应的
 *      vApplicationGetIdleTaskMemory / vApplicationGetTimerTaskMemory
 *      由 firmware/stm32/src/freertos_hooks.c 提供。
 *
 *   2. configUSE_TICKLESS_IDLE = 0
 *      本工程低功耗开关 SMART_LOCK_ENABLE_STOP_MODE 默认为 0（见
 *      smart_lock_board_config.h，docs/bring-up.md 要求先验证唤醒与
 *      时钟恢复）。等启用 STOP 时再一起打开，避免"能进不能出"。
 *
 *   3. 保留 heap_4 的动态堆（cJSON / MqttKit 需要 pvPortMalloc）。
 *      应用层自身不用动态分配。
 */

#define configUSE_PREEMPTION                    1// 开启抢占式调度
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      72000000UL
#define configTICK_RATE_HZ                      1000// 1ms 一个 tick
#define configMAX_PRIORITIES                    5// 最大优先级为 5
#define configMINIMAL_STACK_SIZE                128// 最小栈大小为 128 字节
#define configMAX_TASK_NAME_LEN                 8// 最大任务名长度为 8 字节
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1

/* ---------------- 内存 ---------------- */
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1

/*
 * 动态堆（heap_4）只服务三个调用方：cJSON 解析、MqttKit 报文缓冲、
 * esp8266 下行解包。本工程的任务栈/队列全部走静态分配，
 * 不再从这块堆里拿空间，因此不需要参考工程那样的 16KB。
 *
 * 单片机只有 20KB SRAM，且启动文件已划走 1KB MSP 栈、
 * 应用层静态区已占约 8KB、驱动缓冲约 2KB —— 16KB 堆会让链接失败
 * （armlink L6406E: No space in execution regions）。
 */
#define configTOTAL_HEAP_SIZE                   (4 * 1024)

/* ---------------- 可选特性 ---------------- */
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUES                        1
#define configUSE_QUEUE_SETS                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0

#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelete                     1

/* ---------------- 协程与软件定时器 ---------------- */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         2
/* 2026-09-14 改为 0：全工程没有任何 xTimer* 调用，定时器服务任务
 * （256 字栈 + TCB + 定时器队列 ≈ 1.2 KB RAM）纯属浪费 —— 管理菜单
 * 需要这块 RAM。若日后要用软件定时器，改回 1 即可。 */
#define configUSE_TIMERS                        0
#define configTIMER_TASK_PRIORITY               3
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            256

/* ---------------- 中断优先级（Cortex-M3，4 位） ---------------- */
#define configPRIO_BITS                         4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY        15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* ---------------- 调试与检查 ---------------- */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0
#define configQUEUE_REGISTRY_SIZE               8

#define configASSERT(x) \
    if ((x) == 0) { taskDISABLE_INTERRUPTS(); for (;;) { } }

/* ---------------- 中断向量映射 ---------------- */
/* port.c 用这三个名字定义处理函数，从而落到启动文件的向量表上。
 * 因此 stm32f10x_it.c 里**不要**再定义 SVC/PendSV/SysTick 三个 Handler。 */
#define vPortSVCHandler       SVC_Handler
#define xPortPendSVHandler    PendSV_Handler
#define xPortSysTickHandler   SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
