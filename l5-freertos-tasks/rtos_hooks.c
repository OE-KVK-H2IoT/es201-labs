/* rtos_hooks.c — kernel callbacks the config asks for, shared by both targets.
 *
 * FreeRTOSConfig.h sets configCHECK_FOR_STACK_OVERFLOW = 2, which means the kernel
 * calls vApplicationStackOverflowHook() the moment a task's stack pointer strays
 * past its allotment. On an MCU there is no guard page (see the memory-organization
 * reference), so this hook is how a silent corruption becomes a loud, located stop:
 * we print the offending task's name and park the core for the debugger to catch.
 */
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task;
    printf("!! STACK OVERFLOW in task '%s' — bump its stack in xTaskCreate.\n", name);
    printf("   Parking core %d for GDB (backtrace to see who overflowed).\n",
           (int)portGET_CORE_ID());
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
