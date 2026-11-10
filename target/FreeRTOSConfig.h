#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*
 * FreeRTOS configuration for the no-HAL STM32H743 target.
 *
 * The project must provide a startup file that exports SVC_Handler,
 * PendSV_Handler and SysTick_Handler (the CMSIS names used below).
 */

#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE 0

#define configCPU_CLOCK_HZ 400000000u
#define configTICK_RATE_HZ 1000u
#define configMAX_PRIORITIES 7
#define configMINIMAL_STACK_SIZE 512u
#define configMAX_TASK_NAME_LEN 16
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_TASK_NOTIFICATIONS 1
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configQUEUE_REGISTRY_SIZE 8
#define configUSE_QUEUE_SETS 0
#define configUSE_NEWLIB_REENTRANT 0
#define configENABLE_FPU 1
#define configENABLE_MPU 0
#define configENABLE_TRUSTZONE 0
#define configRUN_FREERTOS_SECURE_ONLY 0
#define configUSE_POSIX_ERRNO 0
#define configUSE_APPLICATION_TASK_TAG 0
#define configUSE_CO_ROUTINES 0
#define configMAX_CO_ROUTINE_PRIORITIES 1

/* Required by the LwIP FreeRTOS port (sys_arch.c) and by the demo tasks. */
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_eTaskGetState 1
#define INCLUDE_xTaskAbortDelay 1
#define INCLUDE_xQueueGetMutexHolder 1
#define INCLUDE_xSemaphoreGetMutexHolder 1

#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 0
#define configTOTAL_HEAP_SIZE (64u * 1024u)
#define configAPPLICATION_ALLOCATED_HEAP 0

#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK 1
#define configUSE_TRACE_FACILITY 0
#define configGENERATE_RUN_TIME_STATS 0
#define configUSE_STATS_FORMATTING_FUNCTIONS 0

#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY 2
#define configTIMER_QUEUE_LENGTH 10
#define configTIMER_TASK_STACK_DEPTH 512u

#define configPRIO_BITS 4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

#define configASSERT(x) \
    do {                \
        if ((x) == 0) { \
            for (;;) {  \
            }           \
        }               \
    } while (0)

/* CMSIS startup vector names. */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif
