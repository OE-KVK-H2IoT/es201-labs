// L3 — generate a square-wave tone on the buzzer (GP13) with the PWM block,
// sweeping the frequency. Shows the wrap/divider math that turns the system
// clock into an output frequency. Put a scope or logic analyzer on GP13 and
// compare the measured frequency to the requested one.
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define BUZZER 13

// Configure the slice for `freq_hz` at 50% duty. Picks a clock divider that
// keeps the 16-bit wrap counter in range for the requested frequency.
static void set_tone(uint slice, uint chan, uint freq_hz) {
    uint32_t f_sys = clock_get_hz(clk_sys);
    float div = 1.0f;
    uint32_t wrap = f_sys / freq_hz - 1;
    while (wrap > 65535) {              // too big for 16 bits: slow the PWM clock
        div += 1.0f;
        wrap = (uint32_t)(f_sys / div) / freq_hz - 1;
    }
    pwm_set_clkdiv(slice, div);
    pwm_set_wrap(slice, (uint16_t)wrap);
    pwm_set_chan_level(slice, chan, (uint16_t)(wrap / 2));   // 50% duty
}

int main(void) {
    stdio_init_all();
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BUZZER);
    uint chan  = pwm_gpio_to_channel(BUZZER);
    pwm_set_enabled(slice, true);

    while (true) {
        for (uint f = 200; f <= 2000; f += 200) {
            set_tone(slice, chan, f);
            printf("tone = %u Hz\n", f);
            sleep_ms(400);
        }
    }
}
