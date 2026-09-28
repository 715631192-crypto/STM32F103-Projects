#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H

#include <stdint.h>

typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;
typedef uint32_t TickType_t;
typedef uint32_t StackType_t;
typedef struct { uint32_t opaque[20]; } StaticQueue_t;
typedef struct { uint32_t opaque[24]; } StaticTask_t;
typedef void *QueueHandle_t;
typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

#define pdTRUE ((BaseType_t)1)
#define pdFALSE ((BaseType_t)0)
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(value) ((TickType_t)(value))
#define taskDISABLE_INTERRUPTS() ((void)0)
#define configMINIMAL_STACK_SIZE 128U
#define configUSE_TIMERS 0
#define configTIMER_TASK_STACK_DEPTH 128U

#endif
