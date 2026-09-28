#ifndef TEST_QUEUE_H
#define TEST_QUEUE_H

#include "FreeRTOS.h"

QueueHandle_t xQueueCreateStatic(UBaseType_t length, UBaseType_t item_size,
                                 uint8_t *storage, StaticQueue_t *control);
BaseType_t xQueueSend(QueueHandle_t queue, const void *item,
                      TickType_t wait_ticks);
BaseType_t xQueueReceive(QueueHandle_t queue, void *item,
                         TickType_t wait_ticks);

#endif
