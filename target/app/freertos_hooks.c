#include "FreeRTOS.h"
#include "task.h"

#include "stm32h7xx.h"

void vApplicationMallocFailedHook(void)
{
    __disable_irq();
    for (;;) {
        __NOP();
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    (void)task_name;

    __disable_irq();
    for (;;) {
        __NOP();
    }
}
