#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* 系统时钟频率 72MHz */
#define configCPU_CLOCK_HZ					( ( unsigned long ) 72000000 )

/* 系统节拍频率 1000Hz = 1ms */
#define configTICK_RATE_HZ					( ( TickType_t ) 1000 )

/* 最大优先级数 */
#define configMAX_PRIORITIES				( 5 )

/* 任务名最大长度 */
#define configMAX_TASK_NAME_LEN				( 16 )

/* 堆栈深度类型 */
#define configSTACK_DEPTH_TYPE				uint16_t

/* 空闲任务堆栈大小 */
#define configMINIMAL_STACK_SIZE			( ( unsigned short ) 128 )

/* 堆总大小 10KB */
#define configTOTAL_HEAP_SIZE				( ( size_t ) ( 10 * 1024 ) )

/* 抢占式调度 */
#define configUSE_PREEMPTION				1

/* 空闲钩子 */
#define configUSE_IDLE_HOOK					0

/* 节拍钩子 */
#define configUSE_TICK_HOOK					0

/* 软件定时器 */
#define configUSE_TIMERS					1
#define configTIMER_TASK_PRIORITY			( 3 )
#define configTIMER_QUEUE_LENGTH			10
#define configTIMER_TASK_STACK_DEPTH		128

/* 计数信号量 */
#define configUSE_COUNTING_SEMAPHORES		1

/* 互斥锁 */
#define configUSE_MUTEXES					1

/* 递归互斥锁 */
#define configUSE_RECURSIVE_MUTEXES			1

/* 中断优先级配置 - Cortex-M3 4位优先级 */
#define configPRIO_BITS						4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY			15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY	5
#define configKERNEL_INTERRUPT_PRIORITY 	( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 	( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* 断言 */
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

/* 16位节拍关闭 */
#define configUSE_16_BIT_TICKS				0

/* 队列注册表大小 */
#define configQUEUE_REGISTRY_SIZE			0

/* 中断重定向 - FreeRTOS接管三个中断 */
#define vPortSVCHandler 	SVC_Handler
#define xPortPendSVHandler 	PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */

#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelayUntil 1
