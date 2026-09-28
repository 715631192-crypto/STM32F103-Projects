#include "FreeRTOS.h"
#include "task.h"

static StaticTask_t idle_task_control;
static StackType_t idle_task_stack[configMINIMAL_STACK_SIZE];
// 为 idle  分配栈内存
void vApplicationGetIdleTaskMemory(StaticTask_t **task_control,
                                   StackType_t **stack,
                                   uint32_t *stack_size)
{
    *task_control = &idle_task_control;
    *stack = idle_task_stack;
    *stack_size = configMINIMAL_STACK_SIZE;
}

#if (configUSE_TIMERS == 1)
static StaticTask_t timer_task_control;
static StackType_t timer_task_stack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory(StaticTask_t **task_control,
                                    StackType_t **stack,
                                    uint32_t *stack_size)
{
    *task_control = &timer_task_control;
    *stack = timer_task_stack;
    *stack_size = configTIMER_TASK_STACK_DEPTH;
}
#endif
// 为任务分配栈内存
void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    (void)task_name;
    taskDISABLE_INTERRUPTS();
    for (;;) {
        /* 调试器可在此查看 task_name。量产机由看门狗复位恢复。 */
    }
}
// 为任务分配动态内存
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) {
        /* 智能门锁应用层不使用动态内存分配。 */
    }
}
