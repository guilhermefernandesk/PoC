/* Mock minimo de FreeRTOS para testar mbtcp.c no host. */
#ifndef FREERTOS_H
#define FREERTOS_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t TickType_t;
typedef long BaseType_t;

#define pdTRUE  1
#define pdFALSE 0
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#define portTICK_PERIOD_MS 1

size_t xPortGetFreeHeapSize(void);

#endif
