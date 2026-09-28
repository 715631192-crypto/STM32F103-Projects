#ifndef SMART_LOCK_FREERTOS_REQUIRED_H
#define SMART_LOCK_FREERTOS_REQUIRED_H

/* 将下面这些配置项合并进 CubeMX 生成的 FreeRTOSConfig.h。 */
#define configSUPPORT_STATIC_ALLOCATION 1
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK 1
#define configUSE_MUTEXES 1
#define configUSE_TASK_NOTIFICATIONS 1
#define configQUEUE_REGISTRY_SIZE 8
#define configUSE_TICKLESS_IDLE 1
#define configUSE_TIMERS 0

#endif
