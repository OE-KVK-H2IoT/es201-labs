// L0 — read a button and narrate over USB-serial.
// Button K1 is active-low (pressed = 0), so we enable the internal pull-up and
// invert. The user LED mirrors the button; the serial line reports each state.
#include <stdio.h>
#include "pico/stdlib.h"

#define BTN_K1 14   // active-low push button, needs an internal pull-up
#define LED1   16   // EP-0172 user LED 1

int main(void) {
    stdio_init_all();               // USB-CDC: open a serial terminal to read this

    gpio_init(LED1);
    gpio_set_dir(LED1, GPIO_OUT);

    gpio_init(BTN_K1);
    gpio_set_dir(BTN_K1, GPIO_IN);
    gpio_pull_up(BTN_K1);           // idle reads 1; pressed pulls to 0

    bool last = false;
    while (true) {
        bool pressed = !gpio_get(BTN_K1);   // invert: active-low
        gpio_put(LED1, pressed);
        if (pressed != last) {              // only print on change
            printf("K1 %s\n", pressed ? "PRESSED" : "released");
            last = pressed;
        }
        sleep_ms(10);                       // simple debounce / poll interval
    }
}
