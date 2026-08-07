/* FreeRTOSConfig.h — the single header that configures the kernel for this lab.
 *
 * It runs no code: the compiler reads it and decides which kernel features exist
 * in the binary at all. This is the "plumbing given complete" from the L5 tutorial
 * (06-freertos-tasks) — get it building once and leave it alone; a few lab tasks
 * flip exactly one macro here and measure what changes.
 *
 * VERSION NOTE: SMP macro names and the import path move between kernel releases.
 * If a build complains about a missing `config...` macro, diff this against the
 * FreeRTOSConfig.h in the FreeRTOS-Kernel repo's RP2350 example — that is ground truth.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* --- Scheduler behaviour ------------------------------------------------- */
#define configUSE_PREEMPTION                    1    /* the point of week 9   */
#define configUSE_TIME_SLICING                  1    /* round-robin equals    */
#define configTICK_RATE_HZ                      1000 /* 1 ms tick             */
#define configMAX_PRIORITIES                    8
#define configMINIMAL_STACK_SIZE                256  /* in WORDS, not bytes   */
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0

/* --- ARMv8-M (Cortex-M33) port requirements ------------------------------ */
#define configENABLE_FPU                        1   /* M33 has a single-prec FPU */
#define configENABLE_MPU                        0   /* not using the MPU here    */
#define configENABLE_TRUSTZONE                  0   /* non-secure (NTZ) port      */
/* Any ISR that calls a FromISR API must run at or below this priority (see the
 * ISR-priority note in the tutorial). 16 matches the Pico SDK FreeRTOS examples. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    16

/* --- SMP: schedule across both Cortex-M33 cores -------------------------- */
#define configNUMBER_OF_CORES                   2
#define configUSE_CORE_AFFINITY                 1    /* vTaskCoreAffinitySet  */
#define configRUN_MULTIPLE_PRIORITIES           1
#define configUSE_TASK_PREEMPTION_DISABLE       0
#define configUSE_PASSIVE_IDLE_HOOK             0

/* --- Memory -------------------------------------------------------------- */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configTOTAL_HEAP_SIZE                   (128 * 1024)
#define configCHECK_FOR_STACK_OVERFLOW          2    /* loud, not silent      */

/* --- Software timers + pended functions ---------------------------------- */
/* Required by the RP2350 SMP port: its Pico-sync interop wakes tasks through an
 * event group set FromISR, which only exists when the timer daemon is enabled. */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)
#define INCLUDE_xTimerPendFunctionCall          1

/* --- Primitives this lab uses -------------------------------------------- */
#define configUSE_MUTEXES                       1    /* priority inheritance  */
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TASK_NOTIFICATIONS            1
#define configQUEUE_REGISTRY_SIZE               8

/* --- Diagnostics: needed for uxTaskGetSystemState ------------------------ */
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1
#define configGENERATE_RUN_TIME_STATS           0

/* --- Pico SDK interop: lets SDK sync/time calls coexist with the kernel --- */
#define configSUPPORT_PICO_SYNC_INTEROP         1
#define configSUPPORT_PICO_TIME_INTEROP         1

#define configCPU_CLOCK_HZ                      150000000 /* RP2350 default */

/* --- API on/off switches -------------------------------------------------- */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_uxTaskGetStackHighWaterMark     1

/* Fail loudly: a tripped assert parks the core where GDB can find it */
#define configASSERT(x) if ((x) == 0) { portDISABLE_INTERRUPTS(); for (;;); }

#endif /* FREERTOS_CONFIG_H */
