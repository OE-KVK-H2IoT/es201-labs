/*
 * a0_first_contact.c — A0: first PicoBot program (reference implementation).
 *
 *   1. says hello on the serial console
 *   2. plays a short tone on the buzzer
 *   3. ramps the motors on the wheels-up stand under failsafe
 *   4. stops feeding and lets the failsafe prove itself
 *
 * Board: PicoBot (RP2350 / Pico 2 W). Helper library: src/picobot/c.
 * SAFETY: run with the wheels clear of the desk, and keep the failsafe armed.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "picobot.h"

int main(void) {
    stdio_init_all();
    sleep_ms(1500);                 /* let USB-CDC attach before the first print */

    printf("\n=== PicoBot A0 first contact ===\n");
    printf("clock: %u Hz\n", (unsigned)clock_get_hz(clk_sys));

    /* 1. buzzer */
    picobot_buzzer_init();
    picobot_buzzer_tone(440);       /* A4 */
    sleep_ms(300);
    picobot_buzzer_tone(523);       /* C5 */
    sleep_ms(300);
    picobot_buzzer_off();
    printf("buzzer: done\n");

    /* 2. motors, under failsafe */
    picobot_motors_init();
    picobot_failsafe_arm(500);      /* no feed for 500 ms -> motors stop */

    for (int i = 0; i < 5; i++) {
        int16_t speed = (int16_t)(60 + i * 30);   /* 60, 90, 120, 150, 180 */
        picobot_motors_set(speed, speed);
        picobot_failsafe_feed();
        printf("motors: %d (age %u ms)\n", speed, (unsigned)picobot_failsafe_age_ms());
        sleep_ms(400);
    }

    /* 3. stop feeding: the failsafe must trip by itself */
    while (!picobot_failsafe_poll()) {
        printf("waiting for failsafe... age %u ms\n", (unsigned)picobot_failsafe_age_ms());
        sleep_ms(200);
    }
    printf("failsafe tripped: motors stopped\n");

    for (;;) {
        tight_loop_contents();
    }
}
