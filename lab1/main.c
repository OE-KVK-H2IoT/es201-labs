/*
 * main.c — Lab 1: first PicoBot program, Pico SDK only (reference implementation).
 *
 * Session 1: hello on the serial console.
 * Session 2: read the four line sensors (GP2-GP5) as GPIO inputs — the sensors
 *            sit behind a comparator, so the RP2350 sees a clean digital level.
 * Then the optional split: this file is the application; picobot.c is the driver.
 *
 * Board: PicoBot (RP2350 / Pico 2 W).
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"   /* clock_get_hz, for the banner */
#include "picobot.h"           /* our driver */

int main(void) {
    stdio_init_all();
    sleep_ms(1500);                 /* let USB-CDC attach before the first print */

    printf("\n=== PicoBot Lab 1 ===\n");
    printf("clock: %u Hz\n", (unsigned)clock_get_hz(clk_sys));

    line_sensors_init();

    for (;;) {
        uint8_t line[LINE_SENSOR_COUNT];
        line_sensors_read(line);
        printf("line: %d %d %d %d  %s\n",
               line[0], line[1], line[2], line[3],
               line_detected() ? "LINE" : "---");
        sleep_ms(200);
    }
}
