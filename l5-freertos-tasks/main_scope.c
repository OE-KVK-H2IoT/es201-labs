/* ==========================================================================
 * L5 · target l5_rtos_scope — the mini-scope as a *system*: two tasks + a queue,
 * with a button ISR delivering the trigger. This is the complete, working
 * reference for tutorial Step 4 — the four `// TODO (n)` gaps students fill in the
 * lab are already filled here (each marked `[solves TODO n]` so you can map the
 * repo back to the tutorial). Read it as the answer key, not the handout.
 *
 *   ADC -> task_sampling -> [ queue ] -> task_display -> ST7796 (L4 driver)
 *              1 kHz                        blocks on queue
 *                        button ISR -> binary semaphore -> freeze/unfreeze
 *
 * Priorities read like an engineer: sampling (4) > display (3) because a late
 * sample is lost data while a late pixel is cosmetics; stats (1) at the bottom so
 * evidence never perturbs the experiment.
 *
 * Build/flash:  ../run.sh l5_rtos_scope      (or see README)
 * ========================================================================== */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "scope_display.h"

#define CORE0_ONLY       ((UBaseType_t)(1 << 0))
#define BUTTON_K1_PIN    14         /* active-low, pull-up — as in L0 */
#define JOYSTICK_X_ADC   0          /* ADC0 = GP26: a signal you can wiggle */
#define SAMPLE_PERIOD_MS 1          /* 1 kHz sampling */
#define QUEUE_DEPTH      64         /* Task 1 attacks this number */

static QueueHandle_t     sample_q;
static SemaphoreHandle_t trigger_sem;

/* Per-task evidence counters — printed once per second by task_stats */
static volatile uint32_t samples_taken = 0;
static volatile uint32_t samples_drawn = 0;
static volatile uint32_t samples_dropped = 0;

/* ---- ISR: record the event, wake a task, get out ----------------------- */
static void button_isr(uint gpio, uint32_t events) {
    (void)gpio; (void)events;
    BaseType_t higher_prio_woken = pdFALSE;

    /* [solves TODO 4] From an ISR you MUST use the FromISR variant and pass the
     * woken flag — a plain xSemaphoreGive here is undefined behaviour. */
    xSemaphoreGiveFromISR(trigger_sem, &higher_prio_woken);

    /* If that give made a higher-priority task ready, switch to it ON EXIT from
     * this ISR instead of waiting for the next tick interrupt. */
    portYIELD_FROM_ISR(higher_prio_woken);
}

/* ---- Producer: hard-periodic, never blocks on the queue ----------------- */
static void task_sampling(void *arg) {
    (void)arg;
    adc_init();
    adc_gpio_init(26 + JOYSTICK_X_ADC);
    adc_select_input(JOYSTICK_X_ADC);

    printf("[sampling] %d Hz, queue depth %d, core %d\n",
           1000 / SAMPLE_PERIOD_MS, QUEUE_DEPTH, (int)portGET_CORE_ID());

    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        uint16_t sample = adc_read();
        samples_taken++;
        /* timeout 0: a scope must never let a slow display stall sampling. If the
         * queue is full, drop and COUNT it — honesty over hope. */
        if (xQueueSend(sample_q, &sample, 0) != pdPASS) {
            samples_dropped++;
        }
        /* [solves TODO 2] Block until exactly last_wake + period, then advance
         * last_wake — the period stays exact no matter how long the work took.
         * (Not vTaskDelay: that would accumulate drift; prove it by reading the
         * printed sample rate below.) */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

/* ---- Consumer: blocks on the queue, renders with the L4 display code ----- */
static void task_display(void *arg) {
    (void)arg;
    bool frozen = false;
    uint16_t sample;

    scope_display_init();    /* L4 path: ST7796 init + baseline */
    printf("[display] ready on core %d\n", (int)portGET_CORE_ID());

    for (;;) {
        /* Non-blocking poll of the trigger between samples: */
        if (xSemaphoreTake(trigger_sem, 0) == pdPASS) {
            frozen = !frozen;
            printf("[display] TRIGGER -> %s\n", frozen ? "FROZEN" : "running");
        }
        /* [solves TODO 3] Receive one sample, BLOCKING until one arrives, but with
         * a 100 ms timeout so the trigger poll above still runs even if the
         * producer dies. Only draw when we actually got one. */
        if (xQueueReceive(sample_q, &sample, pdMS_TO_TICKS(100)) == pdPASS) {
            if (!frozen) {
                scope_draw_sample(sample);
            }
            samples_drawn++;
        }
    }
}

/* ---- Scheduler evidence: who ran, how much, once per second ------------- */
static void task_stats(void *arg) {
    (void)arg;
    TaskStatus_t snap[8];
    const char *state_names[] = {"Running", "Ready", "Blocked",
                                 "Suspended", "Deleted", "Invalid"};
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf("---- 1 s: sampled %lu, drawn %lu, dropped %lu ----\n",
               (unsigned long)samples_taken, (unsigned long)samples_drawn,
               (unsigned long)samples_dropped);
        UBaseType_t n = uxTaskGetSystemState(snap, 8, NULL);
        for (UBaseType_t i = 0; i < n; i++) {
            printf("  %-12s prio %lu  %-9s stack-free %u words\n",
                   snap[i].pcTaskName,
                   (unsigned long)snap[i].uxCurrentPriority,
                   state_names[snap[i].eCurrentState],
                   (unsigned)snap[i].usStackHighWaterMark);
        }
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(2000);
    printf("L5 mini-scope split: sampling -> queue -> display, ISR trigger\n");

    /* [solves TODO 1] Create the producer/consumer link: QUEUE_DEPTH items of
     * sizeof(uint16_t). Item size MUST match what you send/receive — a wrong item
     * size is the classic silent corruptor. If this stays NULL the configASSERT
     * below parks the chip: the kernel telling you the link was never made. */
    sample_q    = xQueueCreate(QUEUE_DEPTH, sizeof(uint16_t));
    trigger_sem = xSemaphoreCreateBinary();
    configASSERT(sample_q && trigger_sem);

    gpio_init(BUTTON_K1_PIN);
    gpio_set_dir(BUTTON_K1_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_K1_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_K1_PIN,
                                       GPIO_IRQ_EDGE_FALL, true, &button_isr);

    TaskHandle_t h_samp, h_disp, h_stat;
    xTaskCreate(task_sampling, "sampling", 1024, NULL, 4, &h_samp);
    xTaskCreate(task_display,  "display",  2048, NULL, 3, &h_disp);
    xTaskCreate(task_stats,    "stats",    1024, NULL, 1, &h_stat);

    /* Week 9 discipline: one core, so the debugger can show you everything.
     * Week 10 (SMP) repins display to core 1 — see the tutorial Part 2. */
    vTaskCoreAffinitySet(h_samp, CORE0_ONLY);
    vTaskCoreAffinitySet(h_disp, CORE0_ONLY);
    vTaskCoreAffinitySet(h_stat, CORE0_ONLY);

    vTaskStartScheduler();
    for (;;) { }
}
