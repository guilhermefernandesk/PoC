/* Mock minimo de task.h para testar mbtcp.c no host. */
#ifndef TASK_H
#define TASK_H

#include "FreeRTOS.h"

TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t ticks);

#endif
