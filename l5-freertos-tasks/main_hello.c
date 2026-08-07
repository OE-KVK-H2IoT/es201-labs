/* ==========================================================================
 * L5 · target l5_rtos_hello — "Hello Tasks", the complete fall-back reference.
 *
 * The one whole, runnable program in the FreeRTOS lab (tutorial Step 3). Flash it
 * first to prove your kernel integration works, and keep it around: whenever the
 * mini-scope split (l5_rtos_scope) misbehaves, come back here, reflash, and confirm
 * the toolchain is still good before blaming your own code.
 *
 * Two tasks, two priorities, two kinds of delay — everything pinned to core 0 so
 * the only source of concurrency is preemption (which the debugger can single-step).
 * As always in this course, the program narrates itself over USB serial.
 *
 * Build/flash:  ../run.sh l5_rtos_hello      (or see README)
 * ========================================================================== */
#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"

#define CORE0_ONLY  ((UBaseType_t)(1 << 0))   /* week-9 affinity mask */

/* Periodic task: vTaskDelayUntil keeps the *period* exact regardless of how long
 * the printing took — the tick numbers advance by exactly 500 every beat. */
static void task_metronome(void *arg) {
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t beat = 0;
    for (;;) {
        printf("[%s] beat %lu at tick %lu on core %d\n",
               pcTaskGetName(NULL), (unsigned long)beat++,
               (unsigned long)xTaskGetTickCount(), (int)portGET_CORE_ID());
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500));
    }
}

/* Chatty task: vTaskDelay sleeps a fixed time *after* the work, so its real period
 * is 700 ms plus however long the work took — the drift is the teaching point. */
static void task_chatter(void *arg) {
    (void)arg;
    for (;;) {
        printf("[%s] hello from priority %u\n",
               pcTaskGetName(NULL), (unsigned)uxTaskPriorityGet(NULL));
        vTaskDelay(pdMS_TO_TICKS(700));
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(2000);   /* let the USB-CDC console attach before we start talking */
    printf("L5 hello-tasks: FreeRTOS SMP kernel, everything pinned to core 0\n");

    TaskHandle_t h_metro, h_chat;
    xTaskCreate(task_metronome, "metronome", 1024, NULL, 3, &h_metro);
    xTaskCreate(task_chatter,   "chatter",   1024, NULL, 2, &h_chat);

    /* Week-9 discipline: one core, so every interleaving is a context switch you
     * can watch. Week 10 (SMP) is where these masks change. */
    vTaskCoreAffinitySet(h_metro, CORE0_ONLY);
    vTaskCoreAffinitySet(h_chat,  CORE0_ONLY);

    vTaskStartScheduler();   /* never returns on success — the kernel owns the CPU */
    printf("scheduler returned: out of heap for the idle task?\n");
    for (;;) { }
}
