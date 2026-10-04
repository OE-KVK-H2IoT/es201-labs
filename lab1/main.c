/*
 * main.c — Lab 1: first PicoBot program, Pico SDK only (reference implementation).
 *
 *   1. blinks the onboard LED (which lives on the CYW43 wireless chip)
 *   2. plays a short tone on the buzzer (our own driver: picobot.c)
 *   3. ramps the motors on the wheels-up stand, under our own failsafe
 *   4. stops feeding and lets the failsafe prove itself
 *
 * Board: PicoBot (RP2350 / Pico 2 W). SAFETY: run with the wheels clear of the
 * desk, on the robot's own supply, and keep the failsafe armed.
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/clocks.h"   /* clock_get_hz, for the banner */
#include "picobot.h"           /* our driver */

int main(void) {
    stdio_init_all();
    sleep_ms(1500);                 /* let USB-CDC attach before the first print */

    if (cyw43_arch_init()) {        /* the LED lives on the wireless chip */
        printf("cyw43 init failed\n");
        return 1;
    }

    printf("\n=== PicoBot Lab 1 first contact ===\n");
    printf("clock: %u Hz\n", (unsigned)clock_get_hz(clk_sys));

    /* 1. blink the onboard LED (twice, as a hello) */
    for (int i = 0; i < 2; i++) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(250);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(250);
    }
    printf("blink: done\n");

    /* 2. buzzer */
    buzzer_init();
    buzzer_tone(440);               /* A4 */
    sleep_ms(300);
    buzzer_tone(523);               /* C5 */
    sleep_ms(300);
    buzzer_off();
    printf("buzzer: done\n");

    /* 3. motors, under our failsafe */
    motors_init();
    failsafe_arm();

    for (int i = 0; i < 5; i++) {
        int16_t speed = (int16_t)(60 + i * 30);
        motors_set(speed, speed);
        failsafe_feed();
        printf("motors: %d (age %u ms)\n", speed, (unsigned)failsafe_age_ms());
        sleep_ms(400);
    }

    /* 4. stop feeding: the failsafe must trip by itself */
    while (failsafe_age_ms() <= 500u) {
        printf("waiting for failsafe... age %u ms\n", (unsigned)failsafe_age_ms());
        sleep_ms(200);
    }
    motors_set(0, 0);
    printf("failsafe tripped: motors stopped\n");

    for (;;) {
        tight_loop_contents();
    }
}
