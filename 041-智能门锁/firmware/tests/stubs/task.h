#ifndef TEST_TASK_H
#define TEST_TASK_H

#include "FreeRTOS.h"

TaskHandle_t xTaskCreateStatic(TaskFunction_t function, const char *name,
                              uint32_t stack_depth, void *argument,
                              UBaseType_t priority, StackType_t *stack,
                              StaticTask_t *control);
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t ticks);

#endif
