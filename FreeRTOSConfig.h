/* Custom FreeRTOSConfig.h
*/
#ifndef __FREERTOS_CONFIG_H
#define __FREERTOS_CONFIG_H

// Avoid including the default esp-open-rtos\FreeRTOS\Source\include\FreeRTOSConfig.h
#define __DEFAULT_FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE. 
 *
 * See http://www.freertos.org/a00110.html.
 *----------------------------------------------------------*/
#define configUSE_PREEMPTION										1
#define configUSE_IDLE_HOOK											0
#define configUSE_TICK_HOOK											0
#define configCPU_CLOCK_HZ			 								( (unsigned long)80000000 )
#define configTICK_RATE_HZ											( (portTickType)XT_TICK_PER_SEC )
#define configMAX_PRIORITIES										( 15 )
// configMINIMAL_STACK_SIZE needs to be >=80, otherwise idle task will overflow.
// Don't know why idle task uses always min stack size?
#define configMINIMAL_STACK_SIZE								( (unsigned short)256 ) 
#define configTOTAL_HEAP_SIZE										( (size_t)( 32 * 1024 ) ) 	// Seems not to change anything
#define configMAX_TASK_NAME_LEN									16
#define configUSE_TRACE_FACILITY								0		// Only enable for debug purpose, not intended for normal use
#define configUSE_STATS_FORMATTING_FUNCTIONS 		0		// Only enable for debug purpose, not intended for normal use
#define configUSE_16_BIT_TICKS									0
#define configIDLE_SHOULD_YIELD									1

#define INCLUDE_xTaskGetIdleTaskHandle 					1
#define INCLUDE_xTimerGetTimerDaemonTaskHandle 	1

#define configCHECK_FOR_STACK_OVERFLOW  				2
#define configUSE_MUTEXES  											1
#define configUSE_RECURSIVE_MUTEXES							1		// Needed for core/newlib_syscalls.c
#define configUSE_TIMERS    										1
#define configUSE_COUNTING_SEMAPHORES						1		// Needed for interrupt driven uart

#if configUSE_TIMERS
	#define configTIMER_TASK_PRIORITY 						( tskIDLE_PRIORITY + 2 )
	#define configTIMER_QUEUE_LENGTH 							10
	#define configTIMER_TASK_STACK_DEPTH  				( (unsigned short )450 )
#endif

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES 									0
#define configMAX_CO_ROUTINE_PRIORITIES 				2
#define configUSE_NEWLIB_REENTRANT 1

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */
#define INCLUDE_vTaskPrioritySet								0
#define INCLUDE_uxTaskPriorityGet								0
#define INCLUDE_vTaskDelete											1
#define INCLUDE_vTaskCleanUpResources						0
#define INCLUDE_vTaskSuspend          					1
#define INCLUDE_vTaskDelayUntil									1
#define INCLUDE_vTaskDelay											1
#define INCLUDE_eTaskGetState 									1		// needed for xTaskAbortDelay
#define INCLUDE_xTaskAbortDelay									1

/*set the #define for debug info*/
#define INCLUDE_xTaskGetCurrentTaskHandle 			1
#define INCLUDE_uxTaskGetStackHighWaterMark 		1
#define INCLUDE_xTaskGetHandle									1

// Ensure to rebuild freertos.a
#ifdef INCLUDE_TRACE
	#include "trace.h" 
	#define traceTASK_SWITCHED_IN()								trace_task_switch()
#endif


#endif /* __FREERTOS_CONFIG_H */

