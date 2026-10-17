/*
 * FreeRTOS Kernel V10.4.x - ModbusDAS Project Configuration
 * Based on original Amazon/FreeRTOS config, modified for STM32F103ZET6
 *
 * RAM Budget (64KB total):
 *   MSP Stack:     4KB  (startup_stm32f103xe.s Stack_Size=0x1000)
 *   C Heap:        1KB  (startup_stm32f103xe.s Heap_Size=0x400)
 *   FreeRTOS Heap: 20KB (configTOTAL_HEAP_SIZE below)
 *   LVGL Pool:    12KB  (lv_conf.h LV_MEM_SIZE)
 *   LVGL DrawBuf: 10KB  (static array in lv_port_disp.c)
 *   Globals:      ~5KB
 *   Margin:       ~12KB
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Core Configuration
 *----------------------------------------------------------*/
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 72000000 )
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                    ( 7 )
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 )
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 20 * 1024 ) )
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configUSE_TRACE_FACILITY                1   /* vTaskList() debugging */
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0   /* F103 no CLZ instruction */

/*-----------------------------------------------------------
 * Synchronization & IPC
 *----------------------------------------------------------*/
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    0   /* not needed, save RAM */
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1

/*-----------------------------------------------------------
 * Software Timers (for periodic sensor read fallback)
 *----------------------------------------------------------*/
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( 2 )
#define configTIMER_QUEUE_LENGTH                8
#define configTIMER_TASK_STACK_DEPTH            ( 256 )

/*-----------------------------------------------------------
 * Debug & Diagnostics
 *----------------------------------------------------------*/
#define configCHECK_FOR_STACK_OVERFLOW          2   /* paint-pattern method */
#define configUSE_MALLOC_FAILED_HOOK            1
#define configGENERATE_RUN_TIME_STATS           0   /* enable later for profiling */
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

/*-----------------------------------------------------------
 * Co-routines (disabled)
 *----------------------------------------------------------*/
#define configUSE_CO_ROUTINES                   0

/*-----------------------------------------------------------
 * Optional API Functions
 *----------------------------------------------------------*/
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_xTaskGetHandle                  1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xQueueGetMutexHolder            1

/*-----------------------------------------------------------
 * ARM Cortex-M3 Interrupt Priorities
 *
 * A派: FreeRTOS port.c handlers mapped to CMSIS vector names.
 * it.c MUST comment out SVC_Handler / PendSV_Handler / SysTick_Handler
 * to avoid duplicate symbol (already done in this project).
 *
 * configMAX_SYSCALL_INTERRUPT_PRIORITY = 191 raw = priority 11 (of 16).
 * Any ISR with priority <= 11 (numerically) MUST NOT call FreeRTOS API.
 * Safe range for FromISR calls: priority 12-15 (raw 192-255).
 *----------------------------------------------------------*/
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

#define configKERNEL_INTERRUPT_PRIORITY         255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    191  /* priority 11 */
#define configLIBRARY_KERNEL_INTERRUPT_PRIORITY 15

#endif /* FREERTOS_CONFIG_H */
